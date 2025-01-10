#include "UserStatistics.h"
#include "utils/utils.h"
#include "utils/constants.h"

namespace v1
{
static const std::string user_score_formula = 
    " (learned_words_total"
    " * learned_words_total / GREATEST(total_learn_days, 1)"
    " * GREATEST(learn_streak, 1)"
    " ) as user_score";

Task<> UserStatistics::getUserStatistics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    Json::Value result_body;
    try {
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto users_db = app().getDbClient(local_constants::users_db);
        auto result = co_await users_db->execSqlCoro("select learned_this_time, last_learn_time, learned_today, learned_words_total,"
            " learn_streak, total_learn_days, last_repeat_time, repeated_today, focus_size_total_sum,"
            " focus_size_changes_total_count, user_rank, last_rank_update_time,"
            + user_score_formula +
            " from user_statistics where user_id = $1", user_id
        );

        auto row = result[0];

	    auto last_learn_time = result[0]["last_learn_time"].as<uint32_t>();
        auto current_time = local_utils::time::get_unix_timestamp();

        // statistics
        auto& statistics = result_body["statistics"];
        statistics["learned_this_time"] = 
            (current_time - last_learn_time > local_constants::statistics::learned_this_time_expire_seconds) 
            ? 0 : row["learned_this_time"].as<uint32_t>();

        auto days_since_current_time = local_utils::time::get_days_passed_since_epoch(current_time);
        auto days_since_last_learn_time = local_utils::time::get_days_passed_since_epoch(last_learn_time);
        auto days_difference_since_last_learn = days_since_current_time - days_since_last_learn_time;

        statistics["learned_today"] = days_difference_since_last_learn == 0 ? row["learned_today"].as<uint32_t>() : 0;
        statistics["repeated_today"] = days_difference_since_last_learn == 0 ? row["repeated_today"].as<uint32_t>() : 0;
        statistics["total_learned_words"] = row["learned_words_total"].as<uint32_t>();

        // performance
        auto& performance = result_body["performance"];
        auto learned_words_total = row["learned_words_total"].as<uint32_t>();
        auto total_learn_days = row["total_learn_days"].as<uint32_t>();

        performance["learned_per_day_on_average"] = total_learn_days != 0 ? learned_words_total / total_learn_days : 0;
        performance["days_streak"] = row["learn_streak"].as<uint32_t>();

        auto focus_size_total_sum = row["focus_size_total_sum"].as<uint64_t>();
        auto focus_size_changes_total_count = row["focus_size_changes_total_count"].as<uint32_t>();
        performance["average_focus_size"] = focus_size_changes_total_count != 0 ? focus_size_total_sum / focus_size_changes_total_count : 0;

        // rank
        auto time_now = local_utils::time::get_unix_timestamp();
        auto last_rank_update_time = row["last_rank_update_time"].as<uint32_t>();
        auto user_rank = row["user_rank"].as<uint16_t>();
        auto user_score = row["user_score"].as<uint64_t>();

        if (time_now - local_constants::global_rank_update_key_expire_seconds > last_rank_update_time) {
            // update user rank
            result = co_await users_db->execSqlCoro(
                " WITH rank_selection AS ("
                "     SELECT rank_pos"
                "     FROM ranks"
                "     WHERE rank_min_score <= $1"
                "     ORDER BY rank_min_score DESC"
                "     LIMIT 1"
                " )"
                " SELECT COALESCE((SELECT rank_pos FROM rank_selection), 100) AS selected_rank",
                user_score
            );
            user_rank = result[0]["selected_rank"].as<uint16_t>();

            co_await users_db->execSqlCoro("update user_statistics "
                "set user_rank = $1, last_rank_update_time = $2 where user_id = $3", user_rank, time_now, user_id
            );

            // try to update global rank
            drogon::async_run([]() -> drogon::Task<void> {
                try {
                    auto rc = drogon::app().getRedisClient();
                    auto global_rank_update_result = co_await rc->execCommandCoro("set %s %s ex %d nx", 
                        local_constants::redis::global_rank_update_key_name.c_str(), 
                        "", 
                        local_constants::global_rank_update_key_expire_seconds
                    );

                    // if key already exists
                    if (global_rank_update_result.isNil()) {
                        co_return;
                    }

                    auto users_db = app().getDbClient(local_constants::users_db);

                    co_await users_db->execSqlCoro(
                       " WITH ranked_scores AS ("
                       "     SELECT "
                       "         user_score,"
                       "         ROW_NUMBER() OVER (ORDER BY user_score DESC) AS row_num,"
                       "         COUNT(*) OVER () AS total_rows"
                       "     FROM ("
                       "         SELECT "
                       + user_score_formula +
                       "         FROM user_statistics"
                       "     ) AS scores"
                       " )"
                       " INSERT INTO ranks (rank_pos, rank_min_score)"
                       " SELECT"
                       " row_num / GREATEST(total_rows / 100, 1) AS rank_pos,"
                       " user_score"
                       " FROM ranked_scores"
                       " WHERE row_num != 0 AND row_num % GREATEST(total_rows / 100, 1) = 0"
                       " ORDER BY user_score DESC"
                       " LIMIT 100"
                       " ON CONFLICT (rank_pos) "
                       " DO UPDATE "
                       " SET rank_min_score = EXCLUDED.rank_min_score"
                    );

                } catch (const std::exception& e) {
                    LOG_ERROR << e.what();
                }
                co_return;
            });
        }

        result_body["rank"] = user_rank;
    } catch (std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_body);
}
}
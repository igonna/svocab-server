#include "queries.h"

namespace core::sql {

namespace user_limits {

drogon::Task<> increase_user_limit_count(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint16_t count,
    const std::string& user_limit_count_name, const std::string& user_limit_last_start_time_name, const uint32_t limit_reset_seconds
) {
    auto user_limits_result = co_await users_db->execSqlCoro(
        "select  " + user_limit_last_start_time_name + " from user_limits where user_id = $1",
        user_id
    );

    auto user_limit_last_start_time = user_limits_result[0][user_limit_last_start_time_name].as<uint32_t>();

    auto current_time = local_utils::time::get_unix_timestamp();

    if (current_time - user_limit_last_start_time > local_constants::user_file_processing_limit::file_processing_limit_reset_seconds) {
        co_await users_db->execSqlCoro("update user_limits set " + user_limit_count_name + " = 1, "
            + user_limit_last_start_time_name + " = $1 where user_id = $2", current_time, user_id
        );
    } else {
        co_await users_db->execSqlCoro("update user_limits set " + user_limit_count_name + " = " + user_limit_count_name + " + $1"
            " where user_id = $2", count, user_id
        );
    }
}

drogon::Task<std::tuple<bool, uint32_t>> is_user_limit_reached(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool user_has_subscription, 
    const std::string& user_limit_count_name, const std::string& user_limit_last_start_time_name,
    const uint32_t limit_reset_seconds, const uint32_t limit_subscriber_user, const uint32_t limit_free_user
) {
    auto user_limits = co_await users_db->execSqlCoro(
        "select " + user_limit_count_name + ", " + user_limit_last_start_time_name + " from user_limits where user_id = $1",
        user_id
    );

    auto current_time = local_utils::time::get_unix_timestamp();

    auto user_limit_count = user_limits[0][user_limit_count_name].as<uint16_t>(); 
    auto user_limit_last_start_time = user_limits[0][user_limit_last_start_time_name].as<uint32_t>();

    if (current_time - user_limit_last_start_time > limit_reset_seconds) {
        co_return {false, 0};
    }

    auto max_generated_phrases = user_has_subscription ? limit_subscriber_user : limit_free_user;

    auto time_limit_resets = user_limit_last_start_time + limit_reset_seconds;

    if (user_limit_count > max_generated_phrases) {
        co_return {true, time_limit_resets};
    } else {
        co_return {false, 0};
    }
}


drogon::Task<> increase_processing_files_count(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint16_t count) {
    co_await increase_user_limit_count(users_db, user_id, count,
        "processed_files_count", "last_processed_file_start_time",
        local_constants::user_file_processing_limit::file_processing_limit_reset_seconds
    );
}

drogon::Task<std::tuple<bool, uint32_t>> is_processing_files_limit_reached(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool user_has_subscription) {
    auto [limit_reached, time_limit_resets] = co_await is_user_limit_reached(users_db, user_id, user_has_subscription, 
        "processed_files_count", "last_processed_file_start_time",
        local_constants::user_file_processing_limit::file_processing_limit_reset_seconds, 
        local_constants::user_file_processing_limit::file_processing_limit_subscriber_user, 
        local_constants::user_file_processing_limit::file_processing_limit_free_user
    );

    co_return {limit_reached, time_limit_resets};
}


drogon::Task<> increase_prompt_generated_topic_count(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint16_t count) {
    co_await increase_user_limit_count(users_db, user_id, count,
        "generated_topics_count", "last_generated_topic_start_time",
        local_constants::user_topic_generation_limit::topic_generation_limit_reset_seconds
    );
}

drogon::Task<std::tuple<bool, uint32_t>> is_prompt_generated_topic_limit_reached(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool user_has_subscription) {
    auto [limit_reached, time_limit_resets] = co_await is_user_limit_reached(users_db, user_id, user_has_subscription, 
        "generated_topics_count", "last_generated_topic_start_time",
        local_constants::user_topic_generation_limit::topic_generation_limit_reset_seconds, 
        local_constants::user_topic_generation_limit::topic_generation_limit_subscriber_user, 
        local_constants::user_topic_generation_limit::topic_generation_limit_free_user
    );

    co_return {limit_reached, time_limit_resets};
}


drogon::Task<> increase_generated_phrases_count(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint16_t count) {
    co_await increase_user_limit_count(users_db, user_id, count,
        "generated_phrases_count", "last_generated_phrase_start_time",
        local_constants::user_generation_limit::generation_limit_reset_seconds
    );
}

drogon::Task<std::tuple<bool, uint32_t>> is_generated_phrases_limit_reached(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool user_has_subscription) {
    auto [limit_reached, time_limit_resets] = co_await is_user_limit_reached(users_db, user_id, user_has_subscription, 
        "generated_phrases_count", "last_generated_phrase_start_time",
        local_constants::user_generation_limit::generation_limit_reset_seconds, 
        local_constants::user_generation_limit::generation_limit_subscriber_user, 
        local_constants::user_generation_limit::generation_limit_free_user
    );

    co_return {limit_reached, time_limit_resets};
}

}

}
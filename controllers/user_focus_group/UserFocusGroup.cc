#include "UserFocusGroup.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"

namespace v1
{

// std::string generate_topic_sequence(
//     const char* src_lang,
//     float mix_value,
//     float skip_value,
//     uint16_t order_words_by,
//     bool exclude_word_if_not_present_in_all_sources,
//     bool exclude_word_if_very_rare,
//     std::vector<std::string> sources_processed
// ) {
//     for (auto& source : sources_processed) {
//         // std::cout << source << std::endl;
//     }

//     // convert text meta to binary meta
//     // source mixing algorithm
//     // convert mix result to text sequence
//     // return result
//     return "";
// }

Task<> UserFocusGroup::getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    Json::Value result_focus_group;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("select last_update_id, last_gen_for_repeat_time from user_sequence "
            "where user_id = $1 and last_gen_id != last_update_id", 
            user_id
        );

        auto sequence_last_update_id = result[0]["last_update_id"].as<uint32_t>();
        auto sequence_last_gen_for_repeat_time = result[0]["last_gen_for_repeat_time"].as<uint32_t>();

        // if generation required
        if (result.size()) {
            auto topics_to_generate = co_await users_db->execSqlCoro(
                "select topic_id, last_update_id from user_topics "
                "where user_id = $1 and last_gen_id != last_update_id and is_enabled = true", 
                user_id
            );

            for (const auto& i : topics_to_generate) {
                auto topic_id = i["topic_id"].as<uint64_t>();
                auto topic_last_update_id = i["last_update_id"].as<uint32_t>();
                auto new_topic_sequence = co_await core::sql::generate_topic_sequence_and_if_update(users_db, user_id, topic_id, topic_last_update_id);

                // debug
                if (topic_id == 27) {
                    co_return local_utils::Response::OK(callback, new_topic_sequence);
                }
            }


            auto topics_sequences = co_await users_db->execSqlCoro(
                "select topic_id from user_topics "
                "where user_id = $1 and is_enabled = true order by src_lang, dst_lang", 
                user_id
            );



            // fetch user topic_words_sequence's
            // and generate final sequence
        } else {
            // just fetch words from sequence
        }

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_focus_group);
}

Task<> UserFocusGroup::updateFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    co_return local_utils::Response::OK(callback, "");
}

}
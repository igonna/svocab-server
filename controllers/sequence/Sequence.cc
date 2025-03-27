#include "Sequence.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"
#include "core/progress_words/progress_words.h"
#include "core/sequence_generation/sequence_generator.h"
#include "plugins/BannedWords/BannedWords.h"
#include "core/user_sequence_words/user_sequence_words.h"
#include "plugins/OpenaiGeneration/OpenaiGeneration.h"
#include "core/phrases_generation/phrases_generator.h"
#include <vector>
#include <utility>
#include "core/user_sequence/user_sequence.h"

namespace v1
{

// Task<bool> Sequence::is_user_blocked_from_generation(uint64_t user_id) {
//     const auto user_blocked_focus_group_generation_key = local_utils::redis::create_blocked_user_focus_group_generation_key(user_id);
//     auto rc = app().getRedisClient();
//     auto user_generation_blocked_result = co_await rc->execCommandCoro(
//         "exists %s",
//         user_blocked_focus_group_generation_key.c_str()
//     );

//     co_return std::stoi(user_generation_blocked_result.asString()) > 0;
// }

// Task<> Sequence::block_user_from_generation(uint64_t user_id) {
//     const auto user_blocked_focus_group_generation_key = local_utils::redis::create_blocked_user_focus_group_generation_key(user_id);
//     auto rc = app().getRedisClient();
//     co_await rc->execCommandCoro("set %s %s ex %d nx",
//         user_blocked_focus_group_generation_key.c_str(),
//         "",
//         local_constants::user_blocked_focus_group_generation_seconds
//     );
//     co_return;
// }

// // is_limit_reached, time_limit_resets
// Task<std::tuple<bool, uint32_t>> Sequence::is_user_generation_limit_reached(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool user_has_subscription) {
//     auto user_phrase_generation_limit_result = co_await users_db->execSqlCoro(
//         "select generated_phrases_count, last_generation_start_time from user_phrase_generation_limit where user_id = $1",
//         user_id
//     );

//     auto current_time = local_utils::time::get_unix_timestamp();

//     auto generated_phrases_count = user_phrase_generation_limit_result[0]["generated_phrases_count"].as<uint16_t>(); 
//     auto last_generation_start_time = user_phrase_generation_limit_result[0]["last_generation_start_time"].as<uint32_t>();

//     if (current_time - last_generation_start_time > local_constants::user_generation_limit::generation_limit_reset_seconds) {
//         co_return {false, 0};
//     }

//     auto max_generated_phrases = user_has_subscription 
//         ? local_constants::user_generation_limit::generation_limit_subscriber_user 
//         : local_constants::user_generation_limit::generation_limit_free_user;

//     auto time_limit_resets = last_generation_start_time + local_constants::user_generation_limit::generation_limit_reset_seconds;

//     if (generated_phrases_count > max_generated_phrases) {
//         co_return {true, time_limit_resets};
//     } else {
//         co_return {false, 0};
//     }
// }

Task<> Sequence::getItem(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        // user sequence info
        auto sequence_info = co_await core::user_sequence::get_sequence_info(users_db, user_id);

        // get user sequence
        auto generated_sequence = co_await core::user_sequence::get_updated_user_sequence(
            users_db, user_id, sequence_info.is_sequence_generation_required, sequence_info.sequence_last_update_id, sequence_info.to_learn_words
        );


        std::cout << "is_sequence_generation_required: " << sequence_info.is_sequence_generation_required 
            << std::endl << "sequence_last_update_id: " << sequence_info.sequence_last_update_id 
            << std::endl << "focus_group_id: " << sequence_info.focus_group_id 
            << std::endl << "focus_group_size: " << sequence_info.focus_group_size 
            << std::endl << "is_focus_group_generation_required: " << sequence_info.is_focus_group_generation_required 
            << std::endl << "focus_group_item (is_null): " << sequence_info.focus_group_item.isNull() 
            << std::endl << "to_learn_words (len): " << sequence_info.to_learn_words.size() 
            << std::endl << "focus_group (is_null): " << sequence_info.focus_group.isNull() 
            << std::endl << "is_focus_group_finished: " << sequence_info.is_focus_group_finished << std::endl;

        // sequence_info

        // get focus group
        // auto focus_group = co_await core::user_sequence::get_updated_focus_group(
        //     users_db, user_id, sequence_info.is_focus_group_generation_required, generated_sequence, sequence_info.focus_group_id,
        //     sequence_info.focus_group
        // );

        // get item
        // 
        // 


        // if body not empty, update progress
        if (req->bodyLength()) {
            auto json = req->getJsonObject();
            // std::cout << json->toStyledString() << std::endl;
        } else {
    
        }


        co_return local_utils::Response::OK(
            callback,
            core::sequence_generation::SequenceGenerator::get_learning_sequence_str(generated_sequence)
        );

        // 2.get_item (requesT from_client):
        // body:
        // {
        //     "action": "passed",
        //     "id": "1.xxa",
        //     "content": {
        //         "mistakes": 0,
        //         "seconds_spent_on_answer": 30
        //     }
        // }

        // 1. generate focus group
        // 2. return current item or status_code


        // is_user_temporarily_blocked_from_phrase_generation

        // is_focus_group_exists_and_not_expired

        // is_user_phrase_generation_limit_reached
        // is_sequence_generation_required
        co_return local_utils::Response::OK(callback, "");
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }

    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}


// Task<> Sequence::getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
//     Json::Value result_focus_group;
//     try {
//         auto users_db = app().getDbClient(local_constants::users_db);
//         auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
//         auto device_num = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::device_num);

//         // check if user not temporarily blocked from generation
//         const auto user_blocked_focus_group_generation_key = local_utils::redis::create_blocked_user_focus_group_generation_key(user_id);
//         auto is_user_blocked_from_generation_result = co_await is_user_blocked_from_generation(user_id);
//         if (is_user_blocked_from_generation_result) {
//             co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, "systems_are_busy");
//         }

//         // return focus_group if not expired && get sequence info
//         // get latest focus group
//         auto group_and_sequence_info_result = co_await users_db->execSqlCoro(
//             " SELECT focus_group_last_update_time, focus_group, last_gen_id, last_update_id, last_gen_time, focus_group_size,"
//             " CASE"
//             " WHEN jsonb_typeof(focus_group->'words') = 'array' THEN jsonb_array_length(focus_group->'words')"
//             " ELSE 0"
//             " END AS words_count FROM user_sequence where user_id = $1", user_id
//         );

//         auto sequence_last_update_id = group_and_sequence_info_result[0]["last_update_id"].as<uint32_t>();
//         auto sequence_last_gen_id = group_and_sequence_info_result[0]["last_gen_id"].as<uint32_t>();
//         auto last_gen_time = group_and_sequence_info_result[0]["last_gen_time"].as<uint32_t>();
//         auto focus_group_size = group_and_sequence_info_result[0]["focus_group_size"].as<uint32_t>();

//         auto words_count = group_and_sequence_info_result[0]["words_count"].as<uint32_t>();
//         auto focus_group_last_update_time = group_and_sequence_info_result[0]["focus_group_last_update_time"].as<uint32_t>();

//         auto current_time = local_utils::time::get_unix_timestamp();

//         bool is_sequence_update_required = current_time - last_gen_time > local_constants::confirmation_level_1_repeat_timeout_seconds 
//             || sequence_last_gen_id != sequence_last_update_id;

//         bool is_focus_group_not_expired = words_count && current_time - focus_group_last_update_time < local_constants::focus_group::focus_group_expire_seconds;

//         // return focus group if not expired && not required completion && sequence update not required
//         if (is_focus_group_not_expired) {
//             auto focus_group_str = group_and_sequence_info_result[0]["focus_group"].c_str();
//             Json::Value parsed_focus_group;

//             if (Json::Reader().parse(focus_group_str, parsed_focus_group)) {
//                 auto parsed_focus_group_summary = get_focus_group_summary(parsed_focus_group);

//                 auto learned_count = parsed_focus_group_summary["summary"]["learned"].as<uint32_t>();
//                 auto failed_count = parsed_focus_group_summary["summary"]["failed"].as<uint32_t>();
//                 auto repeated_count = parsed_focus_group_summary["summary"]["repeated"].as<uint32_t>();

//                 bool is_focus_group_require_phase_completion = learned_count != 0 || failed_count != 0 || repeated_count != 0;

//                 if (is_focus_group_require_phase_completion || !is_sequence_update_required) {
//                     // update focus group current device version
//                     auto device_num_str = std::to_string(device_num);
//                     co_await users_db->execSqlCoro(std::string("UPDATE user_sequence") +
//                         " SET focus_group = jsonb_set(focus_group,'{versions," + device_num_str + "}',"
//                         " to_jsonb($2::int), true) WHERE user_id = $1", user_id, focus_group_last_update_time
//                     );

//                     clear_focus_group_from_technical_data(parsed_focus_group);
//                     co_return local_utils::Response::JSON_OK(callback, parsed_focus_group);
//                 }
//             }
//         }

//         // check if generation limit not reached, and generation not blocked
//         bool user_has_subscription = co_await core::sql::does_user_have_subscription(users_db, user_id);

//         auto [generation_limit_reached, time_limit_resets] = co_await is_user_generation_limit_reached(users_db, user_id, user_has_subscription);

//         // check if user reached phrase generation limit
//         if (generation_limit_reached) {
//             auto time_limit_resets_str = std::to_string(time_limit_resets);
//             co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, time_limit_resets_str.c_str());
//         }

//         // get user words sequence
//         std::vector<core::user_sequence_words::SequenceUserWord> user_words_sequence;

//         if (is_sequence_update_required) {
//             // update if required and get user words sequence
//             user_words_sequence = co_await get_updated_user_words_sequence(users_db, user_id, sequence_last_update_id);
//         } else {
//             // get user words sequence
//             auto user_sequence_result = co_await users_db->execSqlCoro(
//                 "select to_learn_words from user_sequence where user_id = $1", user_id
//             );

//             auto to_learn_words_str = user_sequence_result[0]["to_learn_words"].as<std::string>();
//             user_words_sequence = core::user_sequence_words::sequence_user_words_to_vector(to_learn_words_str);
//         }

//         // generate focus_group_size phrases 
//         std::vector<core::phrases_generator::FocusGroupPhrase> focus_group_phrases;
//         try {
//             focus_group_phrases = co_await generate_focus_group_phrases(
//                 users_db, user_id, user_has_subscription, sequence_last_update_id, user_words_sequence, focus_group_size
//             );

//             if (focus_group_phrases.size() < local_constants::focus_group::min_focus_group_size 
//                 || user_words_sequence.empty() 
//                 || focus_group_phrases.empty()
//             ) {
//                 co_return local_utils::Response::Error::ERROR(callback, k204NoContent, "");
//             }
//         } catch (const focus_group_generation_exceptions::user_blocked_from_generation& e) {
//             co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, "systems_are_busy");
//         } catch (const focus_group_generation_exceptions::systems_are_busy_exception& e) {
//             co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, "systems_are_busy");
//         }

//         // get new focus group id
//         auto new_focus_group_id = co_await get_new_focus_group_id(users_db, user_id);

//         // create json focus group
//         auto user_special_offer_type_opt = co_await get_user_special_offer_type(users_db, user_id);
//         auto user_special_offer_type = user_special_offer_type_opt.has_value() ? user_special_offer_type_opt.value() : "";

//         result_focus_group = create_focus_group_json(current_time, focus_group_phrases, new_focus_group_id, learned_this_time, is_multiple_languages, device_num, user_special_offer_type);

//         // update db focus group
//         auto focus_group_json_str = local_utils::json::json_to_string(result_focus_group);

//         auto update_focus_group_result = co_await users_db->execSqlCoro("update user_sequence set focus_group = $1, "
//             "focus_group_last_update_time = $2 where user_id = $3", focus_group_json_str, current_time, user_id
//         );

//         if (!update_focus_group_result.affectedRows()) {
//             throw std::runtime_error("failed_to_update_focus_group");
//         }
//     } catch (const std::exception& e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Response::Error::SERVER_ERROR(callback);
//     }

//     co_return local_utils::Response::JSON_OK(callback, result_focus_group);
// }
}
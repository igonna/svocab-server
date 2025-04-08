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
#include "core/user_sequence/user_group.h"
#include "core/user_sequence/user_item.h"
#include "utils/exceptions.h"

namespace v1
{
Task<> Sequence::getItem(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    try {
        auto redis_client = app().getRedisClient();
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto is_user_blocked_from_generation = co_await core::user_sequence::is_user_blocked_from_generation(redis_client, user_id);

        if (is_user_blocked_from_generation) {
            co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, "max_gen_attempts_reached");
        }

        auto users_db = app().getDbClient(local_constants::users_db);

        bool did_user_hide_item = false;

        if (req->bodyLength()) {
            auto body_str = std::string(req->getBody());

            Json::Value json_body;

            if (!Json::Reader().parse(body_str, json_body)) {
                co_return local_utils::Response::Error::NOT_VALID_BODY(callback);
            }

            auto update_item_progress_result = co_await core::user_sequence::update_item_progress(
                users_db,
                user_id,
                json_body
            );

            if (update_item_progress_result.has_value()) {
                auto [status_code, message, did_user_hide_item_tmp] = update_item_progress_result.value();
                did_user_hide_item = did_user_hide_item_tmp;

                if (status_code != k200OK) {
                    co_return local_utils::Response::Error::ERROR(callback, status_code, message.c_str());
                }
            }
        }

        // select current item, and group info
        auto group_info = co_await core::user_sequence::UserGroup::get_group_info(users_db, user_id);
        auto current_user_item = co_await core::user_sequence::get_current_user_item(users_db, user_id);

        // TODO: is_focus_group_exists_and_not_expired
        if (current_user_item.has_value()) {
            if (core::user_sequence::ItemType::result == current_user_item.value().type) {
                // insert results to user words
                // and return results to previous scope variable
            }

            // TODO: pass group results
            // TODO: fix phrase splited not on words
            auto current_item_json = co_await current_user_item.value().create_json_user_item();
            co_return local_utils::Response::JSON_OK(callback, current_item_json);
        }

        bool is_group_generation_required = group_info.last_update_id != group_info.last_gen_id || did_user_hide_item;
        if (is_group_generation_required) {
            bool user_has_subscription = false;
    
            auto [phrase_generation_limit_reached, time_limit_resets]
                = co_await core::sql::user_limits::is_generated_phrases_limit_reached(users_db, user_id, user_has_subscription);

            if (phrase_generation_limit_reached) {
                auto time_limit_resets_str = std::to_string(time_limit_resets);
                co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, time_limit_resets_str.c_str());
            }

            auto generated_sequence = co_await core::user_sequence::get_updated_user_sequence(
                users_db, user_id, group_info.last_update_id
            );

            // generate focus_group_size phrases
            bool should_block_user_from_generation = false;
            std::vector<core::phrases_generator::FocusGroupPhrase> group_phrases;
            try {
                group_phrases = co_await core::user_sequence::generate_group_phrases(
                    users_db, user_id, user_has_subscription, generated_sequence, group_info.group_size
                );
            } catch (const exception::user_max_phrase_generation_attempts_reached& e) {
                should_block_user_from_generation = true;
            } catch (const exception::systems_are_busy& e) {
                co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, "systems_are_busy");
            } catch (const exception::not_enough_words_for_group_generation& e) {
                co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "not_enough_words_for_generation");
            }

            if (should_block_user_from_generation) {
                co_await core::user_sequence::block_user_from_generation(redis_client, user_id);
                co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, "max_gen_attempts_reached");
            }

            co_await core::sql::user_limits::increase_generated_phrases_count(users_db, user_id, group_info.group_size);

            bool add_rate_us = co_await core::user_sequence::should_user_get_rate_us_offer(users_db, user_id);
            co_await core::user_sequence::insert_user_items(users_db, user_id, group_phrases, add_rate_us);
        }

        // after generation get user item
        current_user_item = co_await core::user_sequence::get_current_user_item(users_db, user_id);

        if (current_user_item.has_value()) {
            auto current_item_json = co_await current_user_item.value().create_json_user_item();
            co_return local_utils::Response::JSON_OK(callback, current_item_json);
        } else {
            co_return local_utils::Response::Error::ERROR(callback, k204NoContent, "");
        }
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
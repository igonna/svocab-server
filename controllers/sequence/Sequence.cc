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

// enum class ResultType : uint16_t {new, in_learning, mastered};

// drogon::Task<> update_user_words_with_current_group(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t group_id) {
//     auto user_items_result = co_await users_db->execSqlCoro(
//         " select item_type, passted_at, src_lang, dst_lang, word, mistakes_count"
//         " from user_items where user_id = $1 and group_id = $2 and is_passed = TRUE",
//         user_id, group_id
//     );
// }

Task<> Sequence::getItem(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    try {
        auto json_result_body = core::user_sequence::replace_words_with_null("The perforator perforator shit, comes after the perforator shit!", "perforator shit", "en");
        co_return local_utils::Response::JSON_OK(callback, json_result_body);

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

        if (current_user_item.has_value() && !group_info.is_group_expired() && !did_user_hide_item) {
            if (core::user_sequence::ItemType::result == current_user_item.value().type) {
                co_return local_utils::Response::Error::ERROR(callback, k501NotImplemented, "");

                co_await core::sql::user_limits::increase_generated_phrases_count(users_db, user_id, group_info.group_size);
                // auto user_words_update_result = co_await update_user_words_with_current_group(
                //     users_db, user_id, group_info.group_id
                // );
                // insert results to user words
                // and return results to previous scope variable
            }

            // TODO: pass group results
            // TODO: fix phrase splited not on words
            auto current_item_json = co_await current_user_item.value().create_json_user_item();
            co_return local_utils::Response::JSON_OK(callback, current_item_json);
        }

        bool is_group_generation_required = group_info.is_group_expired() || group_info.is_user_added_new_content() || did_user_hide_item;
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
}
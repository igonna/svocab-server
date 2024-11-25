#include "UserFocusGroup.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"
#include "core/progress_words/progress_words.h"
#include "core/sequence_generation/sequence_generator.h"
#include "plugins/BannedWords/BannedWords.h"
#include "core/user_sequence_words/user_sequence_words.h"
#include "plugins/OpenaiPhraseGeneration/OpenaiPhraseGeneration.h"
#include "core/phrases_generation/phrases_generator.h"
#include <vector>
#include <utility>

namespace v1
{

Task<uint32_t> UserFocusGroup::update_focus_group_last_update_time(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto current_time = local_utils::time::get_unix_timestamp();
    auto result = co_await users_db->execSqlCoro("update user_sequence set focus_group_last_update_time = $1"
        " where user_id = $2 returning focus_group_last_update_time", current_time, user_id
    );
    auto new_focus_group_last_update_time = result[0]["focus_group_last_update_time"].as<uint32_t>();
    co_return new_focus_group_last_update_time;
};

Task<> UserFocusGroup::getFocusGroupLastUpdateTime(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro(
            "select focus_group_last_update_time from user_sequence where user_id = $1", user_id
        );

        co_return local_utils::Response::OK(callback, result[0][0].as<std::string>());
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }
}

Task<std::vector<core::user_sequence_words::SequenceUserWord>> UserFocusGroup::get_updated_user_words_sequence(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t sequence_last_update_id) {
    auto banned_words_plugin = app().getPlugin<BannedWords>();
    core::sequence_generation::SequenceGenerator sequence_generator(banned_words_plugin);

    // get user words
    {
        auto user_words_result = co_await core::sql::get_user_words(users_db, user_id);

        for (auto& user_lang_words : user_words_result) {
            auto src_lang = user_lang_words["src_lang"].as<std::string>();
            auto words = user_lang_words["words"].as<std::string>();
            auto last_update_id = user_lang_words["last_update_id"].as<uint32_t>();

            auto [duplicates_count, user_words] = core::progress_words::get_user_words_from_str(words);

            // update words if many duplicates
            if (duplicates_count > static_cast<float>(user_words.size()) * local_constants::compress_user_words_on_percent) {
                auto new_user_words_str = core::progress_words::user_words_to_str(user_words);
                co_await users_db->execSqlCoro(
                    "update user_words set words = $1 where user_id = $2 and last_update_id = $3",
                    new_user_words_str, user_id, last_update_id
                );
            }

            sequence_generator.add_user_words(std::move(src_lang), std::move(user_words));
        }
    }

    // get user topics sequences
    {
        co_await core::sql::update_all_user_topics_sequences(users_db, user_id);

        auto topics_sequences_result = co_await core::sql::get_user_topics_sequences(users_db, user_id);

        for (auto& topic_sequence : topics_sequences_result) {
            auto src_lang = topic_sequence["src_lang"].as<std::string>();
            auto dst_lang = topic_sequence["dst_lang"].as<std::string>();
            auto lang_mix_value = topic_sequence["lang_mix_value"].as<float>();
            auto mix_value = topic_sequence["mix_value"].as<float>();
            auto words_sequence = topic_sequence["words_sequence"].as<std::string>();

            co_await sequence_generator.add_topic_words(std::move(src_lang), std::move(dst_lang), lang_mix_value, mix_value, std::move(words_sequence));
        }
    }

    auto learning_sequence_str = sequence_generator.get_learning_sequence_str();
    auto current_time = local_utils::time::get_unix_timestamp();

    auto user_sequence_words = core::user_sequence_words::sequence_user_words_str_to_vector(learning_sequence_str);

    co_await users_db->execSqlCoro(
        " update user_sequence set last_gen_id = last_update_id, last_gen_time = $1, to_learn_words = $2"
        " where last_update_id = $3 and user_id = $4",
        current_time, learning_sequence_str, sequence_last_update_id, user_id
    );

    co_return user_sequence_words;
}

Task<> UserFocusGroup::increase_generated_phrases_count(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto user_phrase_generation_limit_result = co_await users_db->execSqlCoro(
        "select generated_phrases_count, last_generation_start_time where user_id = $1",
        user_id
    );

    auto generated_phrases_count = user_phrase_generation_limit_result[0]["generated_phrases_count"].as<uint16_t>(); 
    auto last_generation_start_time = user_phrase_generation_limit_result[0]["last_generation_start_time"].as<uint32_t>();

    auto current_time = local_utils::time::get_unix_timestamp();

    if (current_time - last_generation_start_time > local_constants::user_generation_limit::generation_limit_reset_seconds) {
        co_await users_db->execSqlCoro("update user_phrase_generation_limit set generated_phrases_count = 1,"
            " last_generation_start_time = $1 where user_id = $2", current_time, user_id
        );
    }
}

// is_limit_reached, time_limit_resets
Task<std::tuple<bool, uint32_t>> UserFocusGroup::is_user_generation_limit_reached(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool user_has_subscription) {
    auto user_phrase_generation_limit_result = co_await users_db->execSqlCoro(
        "select generated_phrases_count, last_generation_start_time from user_phrase_generation_limit where user_id = $1",
        user_id
    );

    auto current_time = local_utils::time::get_unix_timestamp();

    auto generated_phrases_count = user_phrase_generation_limit_result[0]["generated_phrases_count"].as<uint16_t>(); 
    auto last_generation_start_time = user_phrase_generation_limit_result[0]["last_generation_start_time"].as<uint32_t>();

    if (current_time - last_generation_start_time > local_constants::user_generation_limit::generation_limit_reset_seconds) {
        co_return {false, 0};
    }

    auto max_generated_phrases = user_has_subscription 
        ? local_constants::user_generation_limit::generation_limit_subscriber_user 
        : local_constants::user_generation_limit::generation_limit_free_user;

    auto time_limit_resets = last_generation_start_time + local_constants::user_generation_limit::generation_limit_reset_seconds;

    if (generated_phrases_count > max_generated_phrases) {
        co_return {true, time_limit_resets};
    } else {
        co_return {false, 0};
    }
}

Task<std::optional<std::string>> UserFocusGroup::get_user_special_offer_type(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto is_time_ready_to_show_offer_result = co_await users_db->execSqlCoro("select 1 from last_user_special_offer_time"
        " where user_id = $1 and last_special_offer_time <= NOW() - INTERVAL '5 days'",
        user_id
    );

    if (is_time_ready_to_show_offer_result.empty()) {
        co_return std::nullopt;
    }

    auto user_email_link_required_result = co_await users_db->execSqlCoro("select 1 from users where id = $1 and email is null limit 1",
        user_id
    );

    auto user_reached_required_progress_result = co_await users_db->execSqlCoro("select 1 from user_statistics where user_id = $1 and learned_words_total > 150 limit 1", user_id);
 
    if (user_email_link_required_result.size() && user_reached_required_progress_result.size()) {
        co_await users_db->execSqlCoro("update last_user_special_offer_time set last_special_offer_time = NOW() where user_id = $1", user_id);
        co_return "link_account";
    }

    auto user_rate_us_required_result = co_await users_db->execSqlCoro(
        "select 1 from user_checkpoints where user_id = $1 and rate_us_passed = false limit 1",
        user_id
    );

    if (user_rate_us_required_result.size() && user_reached_required_progress_result.size()) {
        co_await users_db->execSqlCoro("update last_user_special_offer_time set last_special_offer_time = NOW() where user_id = $1", user_id);
        co_return "rate_us";
    }

    co_return std::nullopt;
}

Task<> UserFocusGroup::getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    Json::Value result_focus_group;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        // check if user not temporarily blocked from generation
        const auto user_blocked_focus_group_generation_key = local_utils::redis::create_blocked_user_focus_group_generation_key(user_id);
        auto rc = app().getRedisClient();
        {
            auto user_generation_blocked_result = co_await rc->execCommandCoro(
                "exists %s",
                user_blocked_focus_group_generation_key.c_str()
            );

            if (std::stoi(user_generation_blocked_result.asString()) > 0) {
                co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, "systems_are_busy");
            }
        }

        // return focus_group if not expired
        {
            auto focus_group_info_result = co_await users_db->execSqlCoro(
                " SELECT focus_group_last_update_time, focus_group,"
                " CASE"
                " WHEN jsonb_typeof(focus_group->'words') = 'array' THEN jsonb_array_length(focus_group->'words')"
                " ELSE 0"
                " END AS words_count FROM user_sequence where user_id = $1", user_id
            );

            auto words_count = focus_group_info_result[0]["words_count"].as<uint32_t>();
            auto focus_group_last_update_time = focus_group_info_result[0]["focus_group_last_update_time"].as<uint32_t>();

            auto current_time = local_utils::time::get_unix_timestamp();
            
            if (words_count && current_time - focus_group_last_update_time < local_constants::focus_group::focus_group_expire_seconds) {
                auto focus_group_str = focus_group_info_result[0]["focus_group"].c_str();

                if (Json::Reader().parse(focus_group_str, result_focus_group)) {
                    co_await update_focus_group_last_update_time(users_db, user_id);
                    co_return local_utils::Response::JSON_OK(callback, result_focus_group);
                }
            }
        }

        // check if generation limit not reached, and generation not blocked
        bool user_has_subscription = co_await core::sql::does_user_have_subscription(users_db, user_id);

        auto [generation_limit_reached, time_limit_resets] = co_await is_user_generation_limit_reached(users_db, user_id, user_has_subscription);

        // check if user reached phrase generation limit
        if (generation_limit_reached) {
            auto time_limit_resets_str = std::to_string(time_limit_resets);
            co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, time_limit_resets_str.c_str());
        }

        // get sequence info
        auto sequence_info_result = co_await users_db->execSqlCoro(
            "select last_gen_id, last_update_id, last_gen_time, focus_group_size from user_sequence where user_id = $1", user_id
        );

        auto sequence_last_update_id = sequence_info_result[0]["last_update_id"].as<uint32_t>();
        auto sequence_last_gen_id = sequence_info_result[0]["last_gen_id"].as<uint32_t>();
        auto last_gen_time = sequence_info_result[0]["last_gen_time"].as<uint32_t>();
        auto focus_group_size = sequence_info_result[0]["focus_group_size"].as<uint32_t>();

        // get user words sequence
        std::vector<core::user_sequence_words::SequenceUserWord> user_words_sequence;

        auto current_time = local_utils::time::get_unix_timestamp();

        if (current_time - last_gen_time > local_constants::confirmation_level_1_repeat_timeout_seconds || sequence_last_gen_id != sequence_last_update_id) {
            // update if required and get user words sequence
            user_words_sequence = co_await get_updated_user_words_sequence(users_db, user_id, sequence_last_update_id);
        } else {
            // get user words sequence
            auto user_sequence_result = co_await users_db->execSqlCoro(
                "select to_learn_words from user_sequence where user_id = $1", user_id
            );

            auto to_learn_words_str = user_sequence_result[0]["to_learn_words"].as<std::string>();
            user_words_sequence = core::user_sequence_words::sequence_user_words_str_to_vector(to_learn_words_str);
        }

        auto block_user_from_generation = [&]() -> drogon::Task<> {
            co_await rc->execCommandCoro("set %s %s ex %d nx",
                user_blocked_focus_group_generation_key.c_str(),
                "",
                local_constants::user_blocked_focus_group_generation_seconds
            );
            co_return;
        };

        // generate focus_group_size phrases 
        std::vector<core::phrases_generator::FocusGroupPhrase> focus_group_phrases;
        {
            uint8_t gen_attempts_count = 0;
            constexpr uint8_t max_gen_attempts_limit = 30;

            auto openai_phrase_generation = app().getPlugin<OpenaiPhraseGeneration>();
            auto translations_db = app().getDbClient(local_constants::translations_db);
            core::phrases_generator::PhrasesGenerator phrases_generator(openai_phrase_generation, translations_db);

            // while user_words_sequence not empty OR focus group not ready
            while (user_words_sequence.size() && focus_group_phrases.size() != focus_group_size) {

                // generate phrases from current sequence
                for (auto& user_word : user_words_sequence) {
                    auto to_generate_count = focus_group_size - focus_group_phrases.size();

                    if (!to_generate_count) break;

                    if (user_word.is_excluded || user_word.is_passed) continue;

                    if (++gen_attempts_count >= max_gen_attempts_limit) {
                        // block user for N seconds
                        co_await block_user_from_generation();
                        co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, "systems_are_busy");
                    }

                    phrases_generator.add_phrase(user_word, user_has_subscription);

                    if (phrases_generator.count() == to_generate_count) {
                        try {
                            auto generated_phrases = co_await phrases_generator.get_generated_phrases();
                            focus_group_phrases.insert(focus_group_phrases.end(), 
                                std::make_move_iterator(generated_phrases.begin()), 
                                std::make_move_iterator(generated_phrases.end())
                            );
                        } catch (const OpenaiPhraseGeneration::systems_are_busy_exception& e) {
                            co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, "systems_are_busy");
                        }
                    }
                }

                // at this point focus group must be ready OR user words sequence exhausted that means sequence regeneration required
                if (focus_group_phrases.size() != focus_group_size) {
                    user_words_sequence = co_await get_updated_user_words_sequence(users_db, user_id, sequence_last_update_id);
                }
            }
        }

        if (focus_group_phrases.size() < local_constants::focus_group::min_focus_group_size 
            || user_words_sequence.empty() 
            || focus_group_phrases.empty()
        ) {
            co_return local_utils::Response::Error::ERROR(callback, k204NoContent, "empty_sequence");
        }

        // get new focus group id
        auto new_focus_group_id_result = co_await users_db->execSqlCoro(
            "update user_sequence set focus_group_id = focus_group_id + 1 where user_id = $1 returning focus_group_id",
            user_id
        );
        auto new_focus_group_id = new_focus_group_id_result[0]["focus_group_id"].as<uint32_t>();

        // create json focus group
        result_focus_group = create_focus_group_json(focus_group_phrases, new_focus_group_id);

        // add special offer type to focus group
        auto user_special_offer_type = co_await get_user_special_offer_type(users_db, user_id);
        if (user_special_offer_type.has_value()) {
            result_focus_group["special_offer_type"] = user_special_offer_type.value();
        } else {
            result_focus_group["special_offer_type"] = Json::nullValue;
        }

        // update db focus group
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_focus_group);
}

Json::Value UserFocusGroup::create_focus_group_json(const std::vector<core::phrases_generator::FocusGroupPhrase>& focus_group_phrases, uint32_t focus_group_id) {
    Json::Value result;

    result["id"] = focus_group_id;
    auto& words = result["words"] = Json::arrayValue;

    uint32_t word_num = 0;
    for (auto& phrase : focus_group_phrases) {
        Json::Value new_word;
        new_word["word_id"] = std::to_string(focus_group_id) + '.' + std::to_string(++word_num);
        new_word["src_lang"] = phrase.generated_phrase.src_lang;

        new_word["word"] = phrase.generated_phrase.src_word;

        new_word["word_translation"] = phrase.generated_phrase.dst_word;
        
        auto& guess_phrase = new_word["guess_phrase"] = Json::arrayValue;
        auto phrase_parts = core::text_processing::extract_words_and_punctuation_from_str(phrase.generated_phrase.src_phrase, phrase.generated_phrase.src_lang.c_str());
        std::string phrase_part_to_add;

        for (auto& part : phrase_parts) {
            auto lower_part = core::text_processing::unicode_str_to_lower(part, phrase.generated_phrase.src_lang.c_str());
            if (phrase.generated_phrase.src_word == lower_part) {
                if (phrase_part_to_add.size()) {
                    guess_phrase.append(phrase_part_to_add);
                    phrase_part_to_add.clear();
                }
                guess_phrase.append(Json::nullValue);
            } else {
                phrase_part_to_add += part;
            }
        }

        if (phrase_part_to_add.size()) {
            guess_phrase.append(phrase_part_to_add);
        }

        new_word["phrase"] = phrase.generated_phrase.src_phrase;
        new_word["phrase_translation"] = phrase.generated_phrase.dst_phrase;

        new_word["current_phase"] = 0;

        auto& phases_mistakes = new_word["phases_mistakes"] = Json::arrayValue;
        for (uint32_t i = 0; i < 3; i++) {
            phases_mistakes.append(false);
        }

        auto& guess_words = new_word["guess_words"] = Json::arrayValue;
        for (auto& guess_word : phrase.guess_words) {
            guess_words.append(std::move(guess_word));
        }

        auto& guess_letters = new_word["guess_letters"] = Json::arrayValue;
        for (auto& guess_letter : phrase.guess_letters) {
            guess_letters.append(std::move(guess_letter));
        }

        words.append(new_word);
    }

    return result;
}

Task<> UserFocusGroup::getPreviousFocusGroupResult(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    // request: FocusGroup/get_previous_focus_group_result
    // {
    //     "id": 0,
    //     "mistakes": [
    //         [false, false, false],
    //         [false, false, false],
    //         [false, false, false]
    //     ],
    //     "result": {
    //         "learned": 3,
    //         "failed": 2,
    //         "repeated": 1,
    //         "current_focus_size": 2,
    //         "new_focus_size": 2
    //     }
    // }
    co_return;
}

Task<> UserFocusGroup::getFocusGroupWithHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    // update focus group last update time in redis && db
    co_return;
}

Task<> UserFocusGroup::setWordPhrasePassed(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        if (0/*word_phase == 3 && word_has_no_mistakes*/) {
            co_await increase_generated_phrases_count(users_db, user_id);
        }

    } catch (const std::exception& e) {
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // check if 
    // update focus group last update time in redis && db

    // DESCRIPTION:
    // request: FocusGroup/set_word_phrase_passed
    // focusGroupId 0
    // wordId 0
    // phasePassed 0
    // mistake false
    // last_update_time ?
    co_return;
}

Task<> UserFocusGroup::setSpecialOfferPassed(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto type = req->getHeader("type");

    static const std::unordered_set<std::string> types = {"rate_us", "link_account"};

    if (!types.contains(type)) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "not_valid_type");
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        if (type == "rate_us") {
            auto result = co_await users_db->execSqlCoro(
                "update user_checkpoints set rate_us_passed = true where user_id = $1",
                user_id
            );

            if (!result.affectedRows()) {
                throw std::runtime_error("not_updated");
            }

            result = co_await users_db->execSqlCoro(
                "update user_sequence set focus_group = jsonb_set(focus_group, '{special_offer_type}', 'null'::jsonb, true)"
                " where user_id = $1",
                user_id
            );

            if (!result.affectedRows()) {
                throw std::runtime_error("not_updated");
            }

            // TODO: send new focus_group_last_update_time to client

            co_await update_focus_group_last_update_time(users_db, user_id);
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserFocusGroup::completeFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    // ...
    // update focus group last update time in redis && db

    // DESCRIPTION:
    // request: FocusGroup/complete_focus_group
    // {
    //     // learned || failed || repeated could not exist
    //     "learned": 3,
    //     "failed": 2,
    //     "repeated": 1,
    //     "current_focus_size": 2,
    //     "new_focus_size": 2
    // }
    co_return;
}
}
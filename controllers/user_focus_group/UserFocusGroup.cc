#include "UserFocusGroup.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"
#include "core/progress_words/progress_words.h"
#include "core/sequence_generation/sequence_generator.h"
#include "plugins/BannedWords/BannedWords.h"
#include "core/user_sequence_words/user_sequence_words.h"
#include "plugins/OpenaiPhraseGeneration/OpenaiPhraseGeneration.h"
#include <vector>
#include <utility>

namespace v1
{
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

// reached_limit, time_unlocks
Task<std::tuple<bool, uint32_t>> UserFocusGroup::is_user_generation_limit_reached(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool user_has_subscription) {
    auto user_phrase_generation_limit_result = co_await users_db->execSqlCoro(
        "select generation_waves_passed_today, generated_phrases_count, generation_wave_start_time where user_id = $1",
        user_id
    );

    auto current_time = local_utils::time::get_unix_timestamp();

    auto generation_waves_passed_today = user_phrase_generation_limit_result[0]["generation_waves_passed_today"].as<uint16_t>(); 
    auto generated_phrases_count = user_phrase_generation_limit_result[0]["generated_phrases_count"].as<uint16_t>(); 
    auto generation_wave_start_time = user_phrase_generation_limit_result[0]["generation_wave_start_time"].as<uint32_t>();

    auto current_time_days = local_utils::time::get_days_passed_since_epoch(current_time);
    auto generation_wave_start_time_days = local_utils::time::get_days_passed_since_epoch(generation_wave_start_time);

    if (current_time_days != generation_wave_start_time_days) {
        co_return {false, 0};
    }

    current_time_days;

    auto current_wave_generation_limit = 0;
    if (generation_waves_passed_today) {

    }

    // auto user_has_subscription = does_user_have_subscription();

    // generated_phrases_count

    // 
    if (current_time - local_constants::user_generation_limit::generation_limit_reset_seconds > generation_wave_start_time) {

    }

    // if (generation_waves_passed_today) {

    // }

    if (current_time - generation_wave_start_time) {

    }

    co_return {false, 0};
}

Task<> UserFocusGroup::getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    Json::Value result_focus_group;
    try {
        // check if generation limit not reached, and generation not blocked
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto current_time = local_utils::time::get_unix_timestamp();

        bool user_has_subscription = co_await core::sql::does_user_have_subscription(users_db, user_id);

        auto [generation_limit_reached, time_unlocks] = co_await is_user_generation_limit_reached(users_db, user_id, user_has_subscription);

        // check if user reached phrase generation limit
        if (generation_limit_reached) {
            auto time_unlocks_str = std::to_string(time_unlocks);
            co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, time_unlocks_str.c_str());
        }

        // get sequence info
        auto sequence_info_result = co_await users_db->execSqlCoro(
            "select last_gen_id, last_update_id, last_gen_time, focus_group_size from user_sequence where user_id = $1", user_id
        );

        auto sequence_last_update_id = sequence_info_result[0]["last_update_id"].as<uint32_t>();
        auto sequence_last_gen_id = sequence_info_result[0]["last_gen_id"].as<uint32_t>();
        auto last_gen_time = sequence_info_result[0]["last_gen_time"].as<uint32_t>();
        auto focus_group_size = sequence_info_result[0]["focus_group_size"].as<uint32_t>();

        std::vector<core::user_sequence_words::SequenceUserWord> user_words_sequence;

        // update user words sequence if required and get
        bool was_updated = false;
        if (current_time - last_gen_time > local_constants::confirmation_level_1_repeat_timeout_seconds || sequence_last_gen_id != sequence_last_update_id) {
            auto updated_user_words_sequence = co_await get_updated_user_words_sequence(users_db, user_id, sequence_last_update_id);
            user_words_sequence = std::move(updated_user_words_sequence);
            was_updated = true;
        }

        // get user words sequence
        if (!was_updated && user_words_sequence.empty()) {
            auto user_sequence_result = co_await users_db->execSqlCoro(
                "select focus_group_last_update_time, focus_group_id, focus_group_size, focus_group, to_learn_words"
                " from user_sequence where user_id = $1", user_id
            );

            auto to_learn_words_str = user_sequence_result[0]["to_learn_words"].as<std::string>();
            auto user_sequence_words_vec = core::user_sequence_words::sequence_user_words_str_to_vector(to_learn_words_str);
            user_words_sequence = std::move(user_sequence_words_vec);
        }

        auto openai_phrase_generation = app().getPlugin<OpenaiPhraseGeneration>();

        uint8_t phrase_generation_attempts_count = 0;
        const uint8_t max_phrase_generation_attempts_count = 30;

        std::vector<GeneratedPhrase> focus_group_phrases;

        std::cout << "user words count: " << user_words_sequence.size() << std::endl;

        // TODO: check if old focus_group exists and not expired

        // while 30 attempts not passed (if passed, block user in redis for 3 minutes)
        // try to generate focus_group_size (max 6) phrases, if current user sequence exhausted, update sequence
        // convert generated phrases to JSON
        // set json focus group to db
        // check if special item should be added

        while (user_words_sequence.size() && focus_group_phrases.size() != focus_group_size) {

            if (user_words_sequence.empty()) {
                co_return local_utils::Response::Error::ERROR(callback, k204NoContent, "end_of_sequence");
            }

            std::vector<std::pair<
                core::user_sequence_words::SequenceUserWord*,
                drogon::Task<GeneratedPhrase>*
            >> current_generation_group;

            // generate phrases from current sequence
            for (auto& user_word : user_words_sequence) {
                uint8_t to_generate_count = focus_group_size - focus_group_phrases.size();

                if (to_generate_count == 0) {
                    break;
                }

                if (user_word.is_excluded || user_word.is_passed) {
                    continue;
                }

                if (++phrase_generation_attempts_count >= max_phrase_generation_attempts_count) {
                    co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, "systems_are_busy");
                }

                // create new phrase generation task
                {
                    auto current_generation_task = openai_phrase_generation->generate_phrase(user_word.src_lang, user_word.dst_lang, user_word.word, user_has_subscription);
                    auto generation_pair = std::make_pair(&user_word, &current_generation_task);
                    current_generation_group.push_back(std::move(generation_pair));
                }

                if (current_generation_group.size() == to_generate_count) {

                    // await all phrase generations
                    for (auto& [user_word_ptr, task_ptr] : current_generation_group) {
                        try {
                            auto generated_phrase = co_await *task_ptr;
                            focus_group_phrases.push_back(std::move(generated_phrase));
                        } catch (const OpenaiPhraseGeneration::word_not_valid_exception& e) {
                            user_word_ptr->is_excluded = true;
                        } catch (const OpenaiPhraseGeneration::systems_are_busy_exception& e) {
                            co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, "systems_are_busy");
                        }
                    }

                    current_generation_group.clear();
                }
            }

            if (focus_group_phrases.size() != focus_group_size) {
                // regenerate user sequence
            }
        }

        // TODO: check if phrase generation limit not reached


        // check if phrase_generation_limit > focus_group

        // check if sequence generation required
            // fetch user_words
            // compress user_words if many duplicates
            // update required topics
            // fetch generated topics sequences
            // add user_words and topic_sequences to mixer 
                // when adding words from topic_sequence to language_sequence exclude words that in user_words
                // hidden are hidden and all words that time - last_learn_time > repeat_timeout...
                // ... for current confirmation level considered also hidden, and other are learned
                // ... unordered_map user_words, if user_words.contain(word) continue
                // ... after language sequence formed with size of max_sequence_words exlucde words_to_repeat 
                // ... from user_words and mix them into language_sequence
                // ... mix language sequence to final sequence ['src_lang;dst_lang;word;current_confirmation_level']
            // save sequence and update last_gen_id

        // form current user_focus_group
            // fetch words from user_sequence + lowest_not_passed_word_pointer of focus_size + random_words_count
            // generate phrase for each word
                // get word-phrase from translations
                // check if word not error_word OR move next
                // generate phrase with openai OR move next
                    // check if user paid
                        // generate phrase OR systems_are_busy
                    // check if openai limit not reached
                        // check if there is enough limit for free users
                        // generate phrase OR systems_are_busy
                        // if phrase valid add to translations, if not to error_words
            // set current focus_group
            // update_user_generation_limit on finished pack of phrases generation
            // check if user-based items required to be added to focus group (link email, rate app, etc)
                // check if time - last_user_offer_push_time > user_offer_push_timeout
                    // if added item update last_user_offer_push_time

        // get focus group(user_words_sequence, focus_group_size)
            // get first focus_group_size not passed && not excluded user words from user_words_sequence
                // get_word_phrase for each word simultaneously
                    // 

        result_focus_group = create_focus_group_json(focus_group_phrases, 0); // todo set focus group id
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_focus_group);
}

Json::Value UserFocusGroup::create_focus_group_json(const std::vector<GeneratedPhrase>& focus_group_phrases, uint32_t focus_group_id) {
    Json::Value result;

    result["id"] = focus_group_id;
    auto& words = result["words"] = Json::arrayValue;

    uint32_t word_num = 0;
    for (auto& phrase : focus_group_phrases) {
        Json::Value new_word;
        new_word["word_id"] = std::to_string(focus_group_id) + '.' + std::to_string(++word_num);
        new_word["src_lang"] = phrase.src_lang;

        new_word["word"] = phrase.src_word;
        new_word["word_translation"] = phrase.dst_word;
        new_word["phrase"] = phrase.src_phrase;
        new_word["phrase_translation"] = phrase.dst_phrase;

        new_word["last_phase_passed"] = 0;

        auto& phases_mistakes = new_word["phases_mistakes"] = Json::arrayValue;
        for (uint32_t i = 0; i < 3; i++) {
            phases_mistakes.append(false);
        }

        words.append(new_word);
    }

    return result;

    // {
    //     "id": 1,
    //     "words": [
    //         {
    //             // phase: 1
    //             "word_id": "1.1",
    //             "src_lang": "en",
    //             "word": "perforator",
    //             "word_translation": ["сверильный", "станок"],
    //             "phrase": "The perforator is behind the perforator!",
    //             "phrase_translation": "Cверильный станок стоит за cверильным станком!",
    //             "last_phase_passed": 0,
    //             "phases_mistakes": [false, false, false],
    //             // phase: 2 (additions)
    //             "guess_phrase": ["The ", null, " went into the forest after the ", null, "!"],
    //             "guess_words": ["world", "perforator", "how"],
    //             // phase: 3 (additions)
    //             "guess_letters": ["e", "p", "a", "o", "f", "r", "r", "t", "o", "e", "r"]
    //         }
    //     ],
    //     "special_item": { // could be null
    //         "type": "phrase_generation_limit",
    //         "reset_time_utc": 1727376861
    //     },
    //     "learned_this_time": 0
    // }

    // {
    //     "type": "phrase_generation_limit",
    //     "reset_time_utc": 1727376861
    // },
    // {
    //     "type": "systems_are_busy"
    // },
    // {
    //     "type": "rate_us"
    // },
    // {
    //     "type": "link_account"
    // }
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

Task<> UserFocusGroup::setSpecialItemPassed(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    // get type
    // verify if really passed for specific types
    // return 200 aways when no server error
    // update focus group last update time in redis && db
    co_return;
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
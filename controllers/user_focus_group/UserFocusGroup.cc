#include "UserFocusGroup.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"
#include "core/progress_words/progress_words.h"
#include "core/sequence_generation/sequence_generator.h"

namespace v1
{
Task<> UserFocusGroup::getFocusGroupLastUpdateTime(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
}

Task<> UserFocusGroup::getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
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


    // 1. update required topics


    // 4. generate final sequence
    // 5. update user sequence
    // 6. return focus group

    // TO FINISH END

    // how to mix learned and to_repeat ?

    // check_if_topics_updated -> means_new_generation_required
    // check_if_repeat_time_reached -> means_new_generation_required

    // min focus size && max focus size

    // compress user words (remove earlier duplicates)

    Json::Value result_focus_group;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto sequence_update_info_result = co_await users_db->execSqlCoro(
            "select last_gen_id, last_update_id, last_gen_time from user_sequence where user_id = $1", user_id
        );
        auto sequence_last_update_id = sequence_update_info_result[0]["last_update_id"].as<uint32_t>();
        auto sequence_last_gen_id = sequence_update_info_result[0]["last_gen_id"].as<uint32_t>();
        auto last_gen_time = sequence_update_info_result[0]["last_gen_time"].as<uint32_t>();

        auto current_time = local_utils::time::get_unix_timestamp();

        // sequence generation required if time to repeat or if updated
        if (current_time - last_gen_time > local_constants::confirmation_level_1_repeat_timeout_seconds 
            || sequence_last_gen_id != sequence_last_update_id) {

            core::sequence_generation::SequenceGenerator sequence_generator;

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

                    sequence_generator.add_topic_words(std::move(src_lang), std::move(dst_lang), lang_mix_value, mix_value, std::move(words_sequence));
                }
            }

            auto learning_sequence = sequence_generator.get_learning_sequence();

            co_await users_db->execSqlCoro(
                " update user_sequence set last_gen_id = last_update_id, last_gen_time = $1, to_learn_words = $2"
                " where last_update_id = $3 and user_id = $4",
                current_time, learning_sequence, sequence_last_update_id, user_id
            );

            co_return local_utils::Response::OK(callback, learning_sequence);
        } // sequence update required END

        // TODO: check if old focus_group exists and not expired


        // TopicMixer topic_mixer;

        // for (auto& topic_sequence : topics_sequences_result) {
        //     auto src_lang = topic_sequence["src_lang"].as<std::string>();
        //     auto dst_lang = topic_sequence["dst_lang"].as<std::string>();
        //     auto lang_mix_value = topic_sequence["lang_mix_value"].as<float>();
        //     auto topic_mix_value = topic_sequence["topic_mix_value"].as<float>();
        //     auto topic_words_sequence = topic_sequence["topic_words_sequence"].as<std::string>();

        //     topic_mixer.add_topic(std::move(src_lang), std::move(dst_lang),
        //         lang_mix_value, topic_mix_value, std::move(topic_words_sequence)
        //     );
        // }

        // float current_lang_mix_value = 1.0;
        // float current_topic_mix_value = 1.0;

        // uint32_t words_per_lang = 4;
        // uint32_t words_per_topic = 2;

        // std::vector<std::string> my_vector;
        // auto& x = my_vector.at(0);

        // // law: sum(words_per_topic) <= words_per_lang

        // // for (auto& lang : langs) {
        // //     for (auto& word : words) {

        // //     }
        // // }

        // // array_1, mix_value_1

        // // array_2, mix_value_2

        // // lang_1_word_1, lang_1_word_2, lang_2_word_1, lang_2_word_2

        // // [languages, mix_values] -> [words_sequences, mix_values]

        // std::unordered_map<std::string, std::unordered_map<std::string, std::string>> topics_sequences;


        // // std::vector<>

        // for (auto& topic_sequence : topics_sequences_result) {
            
        // }

        // // sequence generation is just mix topics's sequences

        // // fetch user topic_words_sequence's
        // // and generate final sequence

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_focus_group);
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
}

Task<> UserFocusGroup::getFocusGroupWithHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    // update focus group last update time in redis && db
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
}

Task<> UserFocusGroup::setSpecialItemPassed(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    // get type
    // verify if really passed for specific types
    // return 200 aways when no server error
    // update focus group last update time in redis && db
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
}
}
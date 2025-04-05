#include "core/user_sequence/user_sequence.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"
#include "core/progress_words/progress_words.h"
#include "core/sequence_generation/sequence_generator.h"
#include "plugins/BannedWords/BannedWords.h"
#include "core/user_sequence_words/user_sequence_words.h"
#include "plugins/OpenaiGeneration/OpenaiGeneration.h"
#include "core/phrases_generation/phrases_generator.h"
#include <drogon/HttpController.h>
#include <core/user_sequence_words/user_sequence_words.h>
#include <plugins/OpenaiGeneration/OpenaiGeneration.h>
#include "core/phrases_generation/phrases_generator.h"
#include "utils/exceptions.h"

namespace core::user_sequence {

// drogon::Task<SequenceInfo> get_sequence_info(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool force_fetch_to_learn_words, bool force_fetch_focus_group) {
//     const uint32_t seconds_duration_to_repeat = local_constants::confirmation_level_1_repeat_timeout_seconds;
//     auto current_time = local_utils::time::get_unix_timestamp();

//     const uint32_t seconds_duration_focus_group_expire = local_constants::focus_group::focus_group_expire_seconds;

//     auto user_sequence_result = co_await users_db->execSqlCoro(
//         " WITH conditions AS ("
//             " SELECT"
//                 " (last_gen_id != last_update_id OR $1 - last_gen_time > $2) AND (focus_group_current_item < focus_group_size) AS seq_gen_required,"
//                 " ($1 - focus_group_last_update_time > $3) AS fg_expired,"
//                 " *"
//             " FROM user_sequence"
//             " WHERE user_id = $4"
//         " )"
//         " SELECT"
//             " seq_gen_required,"
//             " last_update_id AS seq_last_update_id,"
//             " focus_group_id AS fg_id,"
//             " focus_group_size AS fg_size,"
//             " fg_expired OR (focus_group_last_item = 0 AND seq_gen_required) AS fg_gen_required,"
//             " focus_group->'d'->(focus_group_current_item % focus_group_size) AS fg_item,"
//             " CASE"
//                 " WHEN fg_expired OR (focus_group_last_item = 0 AND seq_gen_required) AND NOT seq_gen_required OR $5 THEN to_learn_words"
//                 " ELSE ''"
//             " END AS to_learn_words,"
//             " (focus_group_current_item >= focus_group_last_item) AS fg_finished,"
//             " CASE"
//                 " WHEN focus_group_current_item >= focus_group_last_item OR $6 THEN focus_group"
//                 " ELSE NULL"
//             " END AS focus_group"
//         " FROM conditions"
//         , current_time, seconds_duration_to_repeat, seconds_duration_focus_group_expire, user_id, force_fetch_to_learn_words, force_fetch_focus_group
//     );

//     auto focus_group = std::move(user_sequence_result[0]["focus_group"].as<Json::Value>());
//     auto is_focus_group_null = focus_group.isNull();
//     auto focus_group_item = std::move(user_sequence_result[0]["fg_item"].as<Json::Value>());
//     auto to_learn_words = std::move(user_sequence_result[0]["to_learn_words"].as<std::string>());

//     co_return SequenceInfo(
//         user_sequence_result[0]["seq_gen_required"].as<bool>(),
//         user_sequence_result[0]["seq_last_update_id"].as<uint32_t>(),
//         user_sequence_result[0]["fg_id"].as<uint32_t>(),
//         user_sequence_result[0]["fg_size"].as<uint32_t>(),
//         user_sequence_result[0]["fg_gen_required"].as<bool>(),
//         std::move(focus_group_item),
//         std::move(to_learn_words),
//         std::move(focus_group),
//         user_sequence_result[0]["fg_finished"].as<bool>()
//     );
// }

uint32_t get_topic_mix_value(uint32_t topic_index, uint32_t created_at, uint32_t current_time) {
    if (topic_index == 0) return (current_time - created_at > local_constants::topics::seconds_first_topic_prioritized ? 20 : 6);
    else if (topic_index == 1) return 4;
    else if (topic_index == 2) return 3;
    else if (topic_index == 3) return 2;
    else return 1;
}

drogon::Task<std::vector<core::user_sequence_words::SequenceUserWord>> get_updated_user_sequence(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t sequence_last_update_id) {
    auto language_pair_topics = co_await users_db->execSqlCoro(
        " WITH max_lang_pair AS ("
            " SELECT DISTINCT ON (user_id) user_id, src_lang, dst_lang"
            " FROM user_language_pairs"
            " WHERE is_enabled = TRUE AND user_id = $1"
            " ORDER BY user_id, last_enable_time DESC"
        " )"
        " SELECT ut.src_lang, ut.dst_lang, ut.words_sequence, ut.created_at"
        " FROM user_topics ut"
        " JOIN max_lang_pair mlp"
            " ON ut.user_id = mlp.user_id"
            " AND ut.src_lang = mlp.src_lang"
            " AND ut.dst_lang = mlp.dst_lang"
        " WHERE ut.is_enabled = TRUE AND ut.user_id = $1"
        " ORDER BY ut.created_at DESC", user_id
    );

    std::string src_lang;
    std::string dst_lang;

    if (language_pair_topics.empty()) {
        co_return {};
    } else {
        src_lang = language_pair_topics[0]["src_lang"].as<std::string>();
        dst_lang = language_pair_topics[0]["dst_lang"].as<std::string>();
    }

    auto user_words_result = co_await users_db->execSqlCoro(
        " SELECT src_lang, word, is_learned, is_hidden, learn_confirmation_level, last_update_time"
        " FROM user_words WHERE user_id = $1 AND src_lang = $2", user_id, src_lang
    );

    // create sequence generator
    auto banned_words_plugin = drogon::app().getPlugin<BannedWords>();
    core::sequence_generation::SequenceGenerator sequence_generator(banned_words_plugin);

    // add user words
    auto user_words = core::progress_words::get_user_words_from_orm_sql(user_words_result);
    co_await sequence_generator.add_user_words(std::move(src_lang), std::move(user_words));

    auto current_time = local_utils::time::get_unix_timestamp();

    // add topics
    uint32_t topic_index = 0;
    for (auto& topic : language_pair_topics) {
        auto topic_src_lang = topic["src_lang"].as<std::string>();
        auto topic_dst_lang = topic["dst_lang"].as<std::string>();
        auto words_sequence = topic["words_sequence"].as<std::string>();

        auto created_at = topic["created_at"].as<uint32_t>();
        auto lang_mix_value = local_constants::topics::lang_mix_value_multiplier;
        auto topic_mix_value = static_cast<float>(get_topic_mix_value(topic_index++, created_at, current_time));

        co_await sequence_generator.add_topic_words(
            std::move(topic_src_lang),
            std::move(topic_dst_lang),
            lang_mix_value,
            topic_mix_value,
            std::move(words_sequence)
        );
    }

    std::ignore = users_db->execSqlCoro(
        " update user_group set last_gen_id = last_update_id where user_id = $1 and last_update_id = $2",
        user_id, sequence_last_update_id
    );

    co_return sequence_generator.generate();
}

// drogon::Task<> process_item_request(drogon::orm::DbClientPtr users_db, uint64_t user_id, const Json::Value& item_request) {
//     co_return;
// }

// std::tuple<uint32_t, uint32_t> Sequence::decode_word_id(const std::string& word_id_str) {
//     auto dot_pos = word_id_str.find('.');
//     if (dot_pos == std::string::npos) throw std::invalid_argument("invalid format");
//     return {std::stoull(word_id_str.substr(0, dot_pos)), std::stoull(word_id_str.substr(dot_pos + 1))};
// }


// Json::Value Sequence::replace_words_with_null(const std::string& phrase, const std::string& words, const std::string& lang) {
//     Json::Value json_parts = Json::arrayValue;
//     auto phrase_parts = core::text_processing::extract_words_and_punctuation_from_str(phrase, lang.c_str());
//     auto words_to_replace_vec = core::text_processing::extract_words_from_str(words, lang.c_str());
//     std::unordered_set<std::string> words_to_replace(words_to_replace_vec.begin(), words_to_replace_vec.end());
//     auto words_to_replace_count = words_to_replace.size();

//     uint32_t words_to_replace_passed = 0;

//     std::string phrase_part_to_add;
//     for (auto& part : phrase_parts) {
//         auto lower_part = core::text_processing::unicode_str_to_lower(part, lang.c_str());
//         if (words_to_replace.contains(lower_part) || words_to_replace.contains(part)) {
//             ++words_to_replace_passed;

//             if (words_to_replace_passed < words_to_replace_count) {
//                 continue;
//             } else {
//                 words_to_replace_passed = 0;
//             }

//             if (phrase_part_to_add.size()) {
//                 json_parts.append(phrase_part_to_add);
//                 phrase_part_to_add.clear();
//             }
//             json_parts.append(Json::nullValue);
//         } else if (!words_to_replace_passed) {
//             phrase_part_to_add += part;
//         }
//     }

//     if (phrase_part_to_add.size()) {
//         json_parts.append(phrase_part_to_add);
//     }

//     return json_parts;
// }


// Task<> Sequence::increase_generated_phrases_count(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint16_t count) {
//     auto user_phrase_generation_limit_result = co_await users_db->execSqlCoro(
//         "select generated_phrases_count, last_generation_start_time from user_phrase_generation_limit where user_id = $1",
//         user_id
//     );

//     auto generated_phrases_count = user_phrase_generation_limit_result[0]["generated_phrases_count"].as<uint16_t>(); 
//     auto last_generation_start_time = user_phrase_generation_limit_result[0]["last_generation_start_time"].as<uint32_t>();

//     auto current_time = local_utils::time::get_unix_timestamp();

//     if (current_time - last_generation_start_time > local_constants::user_generation_limit::generation_limit_reset_seconds) {
//         co_await users_db->execSqlCoro("update user_phrase_generation_limit set generated_phrases_count = 1,"
//             " last_generation_start_time = $1 where user_id = $2", current_time, user_id
//         );
//     } else {
//         co_await users_db->execSqlCoro("update user_phrase_generation_limit set generated_phrases_count = generated_phrases_count + $1"
//             " where user_id = $2", count, user_id
//         );
//     }
// }


// Task<std::optional<std::string>> Sequence::get_user_special_offer_type(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
//     auto is_time_ready_to_show_offer_result = co_await users_db->execSqlCoro("select 1 from last_user_special_offer_time"
//         " where user_id = $1 and last_special_offer_time <= NOW() - INTERVAL '6 hours'",
//         user_id
//     );

//     if (is_time_ready_to_show_offer_result.empty()) {
//         co_return std::nullopt;
//     }

//     auto user_email_link_required_result = co_await users_db->execSqlCoro("select 1 from users where id = $1 and email is null limit 1",
//         user_id
//     );

//     auto user_reached_required_progress_result = co_await users_db->execSqlCoro("select 1 from user_statistics where user_id = $1 and learned_words_total > 30 limit 1", user_id);
 
//     if (user_email_link_required_result.size() && user_reached_required_progress_result.size()) {
//         co_await users_db->execSqlCoro("update last_user_special_offer_time set last_special_offer_time = NOW() where user_id = $1", user_id);
//         co_return "link_account";
//     }

//     auto user_rate_us_required_result = co_await users_db->execSqlCoro(
//         "select 1 from user_checkpoints where user_id = $1 and rate_us_passed = false limit 1",
//         user_id
//     );

//     if (user_rate_us_required_result.size() && user_reached_required_progress_result.size()) {
//         co_await users_db->execSqlCoro("update last_user_special_offer_time set last_special_offer_time = NOW() where user_id = $1", user_id);
//         co_return "rate_us";
//     }

//     co_return std::nullopt;
// }


// Valid ###Start

// drogon::Task<Json::Value> get_updated_focus_group(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool is_focus_group_generation_required, std::vector<core::user_sequence_words::SequenceUserWord>& sequence_words, const uint32_t focus_group_id, Json::Value& focus_group) {
//     co_return Json::nullValue;
// }

drogon::Task<std::vector<core::phrases_generator::FocusGroupPhrase>> generate_group_phrases(
    drogon::orm::DbClientPtr users_db,
    uint32_t user_id,
    bool user_has_subscription,
    std::vector<core::user_sequence_words::SequenceUserWord>& user_words_sequence,
    uint32_t group_size
) {
    auto openai_phrase_generation = drogon::app().getPlugin<OpenaiGeneration>();
    auto translations_db = drogon::app().getDbClient(local_constants::translations_db);
    core::phrases_generator::PhrasesGenerator phrases_generator(openai_phrase_generation, translations_db);

    uint8_t gen_attempts_count = 0;
    constexpr uint8_t max_gen_attempts_limit = 30;

    std::vector<core::phrases_generator::FocusGroupPhrase> group_phrases;

    for (auto& user_word : user_words_sequence) {
        auto to_generate_count = group_size - group_phrases.size();

        if (to_generate_count == 0) break;

        if (++gen_attempts_count >= max_gen_attempts_limit) {
            throw exception::user_max_phrase_generation_attempts_reached();
        }

        phrases_generator.add_phrase(user_word, user_has_subscription);

        if (phrases_generator.count() == to_generate_count) {
            try {
                auto generated_phrases = co_await phrases_generator.get_generated_phrases();

                for (auto& phrase : generated_phrases) {
                    group_phrases.push_back(std::move(phrase));
                }

            } catch (const OpenaiGeneration::systems_are_busy_exception& e) {
                throw exception::systems_are_busy();
            }
        }
    }

    if (group_phrases.size() < group_size) {
        throw exception::not_enough_words_for_group_generation();
    }

    co_return group_phrases;
}


// Json::Value Sequence::create_focus_group_json(
//     uint32_t current_time,
//     const std::vector<core::phrases_generator::FocusGroupPhrase>& focus_group_phrases, 
//     uint32_t focus_group_id,
//     uint32_t learned_this_time,
//     bool is_multiple_languages,
//     uint32_t device_num,
//     const std::string& special_offer_type
// ) {
//     Json::Value result;

//     result["group_id"] = focus_group_id;
//     auto& versions = result["versions"] = Json::objectValue;
//     versions[std::to_string(device_num)] = current_time;
//     result["is_multiple_languages"] = is_multiple_languages;
//     result["learned_this_time"] = learned_this_time;
//     if (special_offer_type.size()) {
//         result["special_offer_type"] = special_offer_type;
//     } else {
//         result["special_offer_type"] = Json::nullValue;
//     }
//     auto& words = result["words"] = Json::arrayValue;

//     uint32_t word_num = 0;
//     for (auto& phrase : focus_group_phrases) {
//         Json::Value new_word;
//         new_word["word_id"] = std::to_string(focus_group_id) + '.' + std::to_string(word_num++);
//         new_word["confirmation_level"] = phrase.confirmation_level;
//         new_word["src_lang"] = phrase.generated_phrase.src_lang;
//         new_word["src_country"] = local_utils::supported_languages::get_country_code_or_empty(phrase.generated_phrase.src_lang);

//         new_word["word"] = phrase.generated_phrase.src_word;

//         new_word["word_translation"] = phrase.generated_phrase.dst_word;
        
//         auto& guess_phrase = new_word["guess_phrase"] = Json::arrayValue;
//         auto& guess_phrasee = new_word["guess_phrase"] = replace_words_with_null(phrase.generated_phrase.src_phrase, phrase.generated_phrase.src_word, phrase.generated_phrase.src_lang);

//         new_word["phrase"] = phrase.generated_phrase.src_phrase;
//         new_word["phrase_translation"] = phrase.generated_phrase.dst_phrase;
//         new_word["phrase_translation_parts"] = replace_words_with_null(phrase.generated_phrase.dst_phrase, phrase.generated_phrase.dst_word, phrase.generated_phrase.dst_lang);

//         new_word["current_phase"] = 0;

//         auto& phases_mistakes = new_word["phases_mistakes"] = Json::arrayValue;
//         for (uint32_t i = 0; i < 3; i++) {
//             phases_mistakes.append(false);
//         }

//         auto& guess_words = new_word["guess_words"] = Json::arrayValue;
//         for (auto& guess_word : phrase.guess_words) {
//             guess_words.append(std::move(guess_word));
//         }

//         auto& guess_letters = new_word["guess_letters"] = Json::arrayValue;
//         for (auto& guess_letter : phrase.guess_letters) {
//             guess_letters.append(std::move(guess_letter));
//         }

//         auto right_letters = core::text_processing::extract_letters_as_strings(phrase.generated_phrase.src_word, phrase.generated_phrase.src_lang.c_str());
//         auto& right_letters_field = new_word["right_letters"] = Json::arrayValue;
//         for (auto& right_letter : right_letters) {
//             right_letters_field.append(std::move(right_letter));
//         }

//         words.append(new_word);
//     }

//     return result;
// }

// Valid ###END


// Task<> Sequence::getFocusGroupWithHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
//     auto word_id_str = req->getHeader("wordId");

//     if (word_id_str.empty()) {
//         co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
//     }

//     Json::Value result_focus_group;

//     try {
//         auto users_db = app().getDbClient(local_constants::users_db);
//         auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
//         auto device_num = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::device_num);
//         auto current_time = local_utils::time::get_unix_timestamp();

//         // check if user not temporarily blocked from generation
//         const auto user_blocked_focus_group_generation_key = local_utils::redis::create_blocked_user_focus_group_generation_key(user_id);
//         auto is_user_blocked_from_generation_result = co_await is_user_blocked_from_generation(user_id);
//         if (is_user_blocked_from_generation_result) {
//             co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, "systems_are_busy");
//         }

//         auto old_focus_group_result = co_await users_db->execSqlCoro(
//             "select focus_group, last_update_id, last_gen_id, last_gen_time, focus_group_size from user_sequence where user_id = $1", user_id
//         );

//         if (old_focus_group_result.empty()) {
//             throw std::runtime_error("old focus group is empty");
//         }

//         auto old_focus_group = old_focus_group_result[0]["focus_group"].as<Json::Value>();
        
//         std::string hidden_word;
//         std::string new_hidden_user_word;
//         std::string new_hidden_word_src_lang;
//         uint32_t hidden_word_current_phase = 0;
        
//         const auto& words = old_focus_group["words"];
//         for (auto& word : words) {
//             if (word_id_str == word["word_id"].as<std::string>()) {
//                 hidden_word = word["word"].as<std::string>();
//                 new_hidden_user_word = core::progress_words::create_hidden_word_str(hidden_word, true, current_time);
//                 new_hidden_word_src_lang = word["src_lang"].as<std::string>();
//                 hidden_word_current_phase = word["current_phase"].as<uint32_t>();
//             }
//         }

//         if (hidden_word_current_phase > 0) {
//             co_await increase_generated_phrases_count(users_db, user_id, 1);
//         }

//         // add new hidden word
//         co_await users_db->execSqlCoro("update user_words set words = words || $1 where src_lang = $2 and user_id = $3",
//             new_hidden_user_word, new_hidden_word_src_lang, user_id
//         );

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

//         auto sequence_last_update_id = old_focus_group_result[0]["last_update_id"].as<uint32_t>();
//         auto sequence_last_gen_id = old_focus_group_result[0]["last_gen_id"].as<uint32_t>();
//         auto last_gen_time = old_focus_group_result[0]["last_gen_time"].as<uint32_t>();
//         auto focus_group_size = old_focus_group_result[0]["focus_group_size"].as<uint32_t>();

//         bool is_sequence_update_required = current_time - last_gen_time > local_constants::confirmation_level_1_repeat_timeout_seconds 
//             || sequence_last_gen_id != sequence_last_update_id;

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

//         // set hidden word as passed in user words sequence and update user words sequence
//         for (auto& user_word : user_words_sequence) {
//             if (user_word.is_passed) continue;

//             if (user_word.word == hidden_word && user_word.src_lang == new_hidden_word_src_lang) {
//                 user_word.is_passed = true;
//                 break;
//             }
//         }

//         auto to_learn_words_updated_str = core::user_sequence_words::sequence_user_words_to_str(user_words_sequence);
//         co_await users_db->execSqlCoro(
//             "update user_sequence set to_learn_words = $1 where last_update_id = $2 and user_id = $3",
//             to_learn_words_updated_str, sequence_last_update_id, user_id
//         );

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
//         auto is_multiple_languages = co_await get_is_multiple_languages(users_db, user_id);
//         auto user_special_offer_type_opt = co_await get_user_special_offer_type(users_db, user_id);
//         auto user_special_offer_type = user_special_offer_type_opt.has_value() ? user_special_offer_type_opt.value() : "";
//         auto learned_this_time = co_await get_learned_this_time(users_db, user_id);

//         result_focus_group = create_focus_group_json(current_time, focus_group_phrases, new_focus_group_id, learned_this_time, is_multiple_languages, device_num, user_special_offer_type);

//         // update new focus group with old results
//         auto& new_words = result_focus_group["words"];

//         if (!old_focus_group.isMember("words")) {
//             old_focus_group["words"] = Json::arrayValue;
//         }
//         auto& old_words = old_focus_group["words"];

//         for (auto& new_word : new_words) {
//             auto new_word_str = new_word["word"].as<std::string>();
//             auto new_word_src_lang = new_word["src_lang"].as<std::string>();
//             for (auto& old_word : old_words) {
//                 auto old_word_str = old_word["word"].as<std::string>();
//                 auto old_word_src_lang = old_word["src_lang"].as<std::string>();

//                 if (new_word_str == old_word_str && new_word_src_lang == old_word_src_lang) {
//                     new_word["current_phase"] = old_word["current_phase"];
//                     new_word["phases_mistakes"] = old_word["phases_mistakes"];
//                 }
//             }
//         }

//         // update db focus group
//         auto focus_group_json_str = local_utils::json::json_to_string(result_focus_group);

//         clear_focus_group_from_technical_data(result_focus_group);

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





// Task<> Sequence::setCurrentWordPhase(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
//     auto word_id_str = req->getHeader("wordId");
//     auto phase_num_str = req->getHeader("phaseNum");
//     auto mistake_str = req->getHeader("madeMistake");

//     if (word_id_str.empty() || phase_num_str.empty() || mistake_str.empty()) {
//         co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
//     }

//     try {
//         auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
//         auto device_num = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::device_num);

//         auto device_num_str = std::to_string(device_num);

//         auto [focus_group_id, word_index] = decode_word_id(word_id_str);

//         auto current_time = local_utils::time::get_unix_timestamp();

//         auto phase_index_str = std::to_string(std::stoi(phase_num_str) - 1);

//         auto new_version_str = std::to_string(current_time);

//         auto transaction = co_await app().getDbClient(local_constants::users_db)->newTransactionCoro();

//         auto update_word_phrase_result = co_await transaction->execSqlCoro(
//             " WITH selected_rows AS ("
//                 " SELECT focus_group, focus_group_last_update_time"
//                 " FROM user_sequence"
//                 " WHERE user_id = $2 AND focus_group_id = $3"
//                 " FOR UPDATE"
//             " )"
//             " UPDATE user_sequence SET focus_group = jsonb_set("
//                 " CASE "
//                     " WHEN COALESCE((focus_group #>> '{versions," + device_num_str + "}')::int, -1) = focus_group_last_update_time"
//                     " THEN jsonb_set(focus_group, '{versions," + device_num_str + "}', '" + new_version_str + "'::jsonb)"
//                     " ELSE focus_group"
//                 " END,"
//                 " '{words}',"
//                 " ("
//                     " SELECT jsonb_agg("
//                         " CASE "
//                             " WHEN word->>'word_id' = '" + word_id_str + "' "
//                             " THEN jsonb_set(jsonb_set(word, '{current_phase}', '" + phase_num_str + "'::jsonb), '{phases_mistakes," + phase_index_str + "}', '" + mistake_str + "'::jsonb)"
//                             " ELSE word"
//                         " END"
//                     " ) "
//                     " FROM jsonb_array_elements(focus_group->'words') AS word"
//                 " )"
//             " )"
//             " , focus_group_last_update_time = $1 WHERE user_id = $2 AND focus_group_id = $3", current_time, user_id, focus_group_id
//         );

//         transaction->setCommitCallback([moved_callback = std::move(callback)](bool success) -> void {
//             if (success) {
//                 auto resp = drogon::HttpResponse::newHttpResponse();
//                 resp->setStatusCode(drogon::k200OK);
//                 moved_callback(resp);
//             } else {
//                 auto resp = drogon::HttpResponse::newHttpResponse();
//                 resp->setStatusCode(drogon::k500InternalServerError);
//                 resp->setBody("something_wrong");
//                 moved_callback(resp);
//             }
//         });

//         if (!update_word_phrase_result.affectedRows()) {
//             throw std::runtime_error("no_word_was_updated");
//         }
//     } catch (const std::exception& e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Response::Error::SERVER_ERROR(callback);
//     }
// }

}
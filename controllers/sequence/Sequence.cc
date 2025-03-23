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
Task<> Sequence::getItem(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    if (req->bodyLength()) {
        auto json = req->getJsonObject();
        std::cout << json->toStyledString() << std::endl;
    } else {

    }



    // 1. just check if generation required, if yes, generate sequence
    // 2. then you allowed to generate focus group

    // get 
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto sequence_info = co_await core::user_sequence::get_sequence_info(users_db, user_id);

        if (sequence_info.is_sequence_generation_required) {
            auto generated_sequence = co_await core::user_sequence::generate_user_sequence_and_update(
                users_db, user_id, sequence_info.is_sequence_generation_required
            );

            co_return local_utils::Response::OK(callback, "new sequence generated");
        } else {
            co_return local_utils::Response::OK(callback, "sequence already generated");
        }

        // if body not empty, update progress



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
//             user_words_sequence = core::user_sequence_words::sequence_user_words_str_to_vector(to_learn_words_str);
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
//         auto is_multiple_languages = co_await get_is_multiple_languages(users_db, user_id);
//         auto user_special_offer_type_opt = co_await get_user_special_offer_type(users_db, user_id);
//         auto user_special_offer_type = user_special_offer_type_opt.has_value() ? user_special_offer_type_opt.value() : "";
//         auto learned_this_time = co_await get_learned_this_time(users_db, user_id);

//         result_focus_group = create_focus_group_json(current_time, focus_group_phrases, new_focus_group_id, learned_this_time, is_multiple_languages, device_num, user_special_offer_type);

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








// Task<uint32_t> Sequence::update_focus_group_last_update_time(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
//     auto current_time = local_utils::time::get_unix_timestamp();
//     auto result = co_await users_db->execSqlCoro("update user_sequence set focus_group_last_update_time = $1"
//         " where user_id = $2 returning focus_group_last_update_time", current_time, user_id
//     );
//     auto new_focus_group_last_update_time = result[0]["focus_group_last_update_time"].as<uint32_t>();
//     co_return new_focus_group_last_update_time;
// };

// Task<> Sequence::isUpdated(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
//     try {
//         auto users_db = app().getDbClient(local_constants::users_db);
//         auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
//         auto device_num = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::device_num);
//         auto device_num_str = std::to_string(device_num);

//         auto result = co_await users_db->execSqlCoro(
//             "SELECT 1 FROM user_sequence WHERE user_id = $1"
//             " AND (focus_group_last_update_time IS DISTINCT FROM"
//             " COALESCE(NULLIF(focus_group->'versions'->>$2, '')::integer, NULL) OR NOT (focus_group->'versions' ? $2))"
//             " AND (focus_group->'group_id' IS NOT NULL) FOR SHARE",
//             user_id, device_num_str
//         );

//         if (result.size()) {
//             co_return local_utils::Response::OK(callback, "");
//         } else {
//             co_return local_utils::Response::Error::ERROR(callback, k304NotModified, "");
//         }

//     } catch (const std::exception& e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Response::Error::SERVER_ERROR(callback);
//     }
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

// Task<uint32_t> Sequence::get_new_focus_group_id(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
//     auto new_focus_group_id_result = co_await users_db->execSqlCoro(
//         "update user_sequence set focus_group_id = focus_group_id + 1 where user_id = $1 returning focus_group_id",
//         user_id
//     );
//     auto new_focus_group_id = new_focus_group_id_result[0]["focus_group_id"].as<uint32_t>();
//     co_return new_focus_group_id;
// }

// Task<std::vector<core::phrases_generator::FocusGroupPhrase>> Sequence::generate_focus_group_phrases(
//     drogon::orm::DbClientPtr users_db,
//     uint32_t user_id,
//     bool user_has_subscription,
//     uint32_t sequence_last_update_id,
//     std::vector<core::user_sequence_words::SequenceUserWord>& user_words_sequence,
//     uint32_t focus_group_size
// ) {
//     // generate focus_group_size phrases 
//     std::vector<core::phrases_generator::FocusGroupPhrase> focus_group_phrases;
    
//     uint8_t gen_attempts_count = 0;
//     constexpr uint8_t max_gen_attempts_limit = 30;

//     auto openai_phrase_generation = app().getPlugin<OpenaiGeneration>();
//     auto translations_db = app().getDbClient(local_constants::translations_db);
//     core::phrases_generator::PhrasesGenerator phrases_generator(openai_phrase_generation, translations_db);

//     bool sequence_regenerated = false;
//     std::vector<core::user_sequence_words::SequenceUserWord> old_user_words_sequence;

//     std::unordered_set<std::string> src_words_already_added;

//     // while user_words_sequence not empty OR focus group not ready
//     while (user_words_sequence.size() && focus_group_phrases.size() != focus_group_size) {

//         // generate phrases from current sequence
//         for (auto& user_word : user_words_sequence) {
//             auto to_generate_count = focus_group_size - focus_group_phrases.size();

//             if (!to_generate_count) break;

//             if (user_word.is_passed) continue;

//             if (++gen_attempts_count >= max_gen_attempts_limit) {
//                 // block user for N seconds
//                 co_await block_user_from_generation(user_id);
//                 throw focus_group_generation_exceptions::user_blocked_from_generation();
//             }

//             phrases_generator.add_phrase(user_word, user_has_subscription);

//             if (phrases_generator.count() == to_generate_count) {
//                 try {
//                     auto generated_phrases = co_await phrases_generator.get_generated_phrases();

//                     for (auto& phrase : generated_phrases) {
//                         std::string src_word_key = phrase.generated_phrase.src_lang + phrase.generated_phrase.src_word;

//                         if (src_words_already_added.contains(src_word_key)) continue;

//                         focus_group_phrases.push_back(std::move(phrase));
//                         src_words_already_added.insert(src_word_key);
//                     }

//                 } catch (const OpenaiGeneration::systems_are_busy_exception& e) {
//                     throw focus_group_generation_exceptions::systems_are_busy_exception();
//                 }
//             }
//         }

//         // at this point focus group must be ready OR user words sequence exhausted that means sequence regeneration required
//         if (focus_group_phrases.size() != focus_group_size && !sequence_regenerated) {
//             old_user_words_sequence = std::move(user_words_sequence);
//             user_words_sequence = co_await get_updated_user_words_sequence(users_db, user_id, sequence_last_update_id);
//             sequence_regenerated = true;
//         } else {
//             break;
//         }
//     }

//     co_return focus_group_phrases;
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

// void Sequence::clear_focus_group_from_technical_data(Json::Value& focus_group) {
//     focus_group.removeMember("versions");

//     if (focus_group.isMember("words")) {
//         auto& words = focus_group["words"];

//         for (auto& word : words) {
//             word.removeMember("confirmation_level");
//         }
//     }
// }

// Task<> Sequence::getPreviousFocusGroupResult(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
//     Json::Value previous_focus_group_json;
//     try {
//         auto users_db = app().getDbClient(local_constants::users_db);
//         auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

//         auto previous_focus_group_result = co_await users_db->execSqlCoro(
//             "select previous_focus_group_result from user_sequence where user_id = $1",
//             user_id
//         );

//         previous_focus_group_json = previous_focus_group_result[0]["previous_focus_group_result"].as<Json::Value>();

//         if (previous_focus_group_json.isObject() && previous_focus_group_json.empty()) {
//             co_return local_utils::Response::Error::ERROR(callback, k204NoContent, "");
//         }
//     } catch (const std::exception& e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Response::Error::SERVER_ERROR(callback);
//     }
//     co_return local_utils::Response::JSON_OK(callback, previous_focus_group_json);
// }

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
//             user_words_sequence = core::user_sequence_words::sequence_user_words_str_to_vector(to_learn_words_str);
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

// std::tuple<uint32_t, uint32_t> Sequence::decode_word_id(const std::string& word_id_str) {
//     auto dot_pos = word_id_str.find('.');
//     if (dot_pos == std::string::npos) throw std::invalid_argument("invalid format");
//     return {std::stoull(word_id_str.substr(0, dot_pos)), std::stoull(word_id_str.substr(dot_pos + 1))};
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

// Task<> Sequence::setSpecialOfferPassed(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
//     auto type = req->getHeader("type");

//     static const std::unordered_set<std::string> types = {"rate_us", "link_account"};

//     if (!types.contains(type)) {
//         co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "not_valid_type");
//     }

//     std::string focus_group_last_update_time_str;
//     try {
//         auto users_db = app().getDbClient(local_constants::users_db);
//         auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

//         if (type == "rate_us") {
//             auto result = co_await users_db->execSqlCoro(
//                 "update user_checkpoints set rate_us_passed = true where user_id = $1",
//                 user_id
//             );

//             if (!result.affectedRows()) {
//                 throw std::runtime_error("not_updated");
//             }

//             result = co_await users_db->execSqlCoro(
//                 "update user_sequence set focus_group = jsonb_set(focus_group, '{special_offer_type}', 'null'::jsonb, true)"
//                 " where user_id = $1",
//                 user_id
//             );

//             if (!result.affectedRows()) {
//                 throw std::runtime_error("not_updated");
//             }

//             auto update_time = co_await update_focus_group_last_update_time(users_db, user_id);
//             focus_group_last_update_time_str = std::to_string(update_time);
//         }
//     } catch (const std::exception& e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Response::Error::SERVER_ERROR(callback);
//     }

//     co_return local_utils::Response::OK(callback, focus_group_last_update_time_str.c_str());
// }

// Json::Value Sequence::get_focus_group_summary(const Json::Value& focus_group) {
//     uint32_t learned_count = 0;
//     uint32_t failed_count = 0;
//     uint32_t repeated_count = 0;

//     uint32_t current_focus_size = 0;

//     Json::Value summary;

//     if (focus_group.isMember("words")) {
//         summary["group_id"] = focus_group["group_id"];
//         auto& words_phases_mistakes = summary["words_phases_mistakes"] = Json::arrayValue;

//         auto& words = focus_group["words"];

//         current_focus_size = words.size();

//         for (auto& word : words) {
//             const auto& phases_mistakes = word["phases_mistakes"];

//             bool word_has_mistake = false;

//             for (const auto &phase_mistake : phases_mistakes) {
//                 if (phase_mistake.asBool()) {
//                     word_has_mistake = true;
//                 }
//             }

//             auto current_phase = word["current_phase"].as<uint32_t>();

//             if (current_phase != 0) {
//                 if (word_has_mistake) {
//                     ++failed_count;
//                 } else {
//                     auto confirmation_level = word["confirmation_level"].as<uint32_t>();
//                     if (confirmation_level > 0) {
//                         ++repeated_count;
//                     } else {
//                         ++learned_count;
//                     }
//                 }
//             }

//             words_phases_mistakes.append(phases_mistakes);
//         }
//     }

//     int32_t new_focus_size = current_focus_size;

//     if (learned_count + repeated_count > failed_count && current_focus_size < local_constants::focus_group::max_focus_group_size) {
//         ++new_focus_size;
//     } else if (learned_count + repeated_count < failed_count && current_focus_size > local_constants::focus_group::min_focus_group_size) {
//         --new_focus_size;
//     }

//     auto& summary_field = summary["summary"] = Json::objectValue;
//     summary_field["learned"] = learned_count;
//     summary_field["failed"] = failed_count;
//     summary_field["repeated"] = repeated_count;
//     summary_field["current_focus_size"] = current_focus_size;
//     summary_field["new_focus_size"] = new_focus_size;
//     summary_field["max_focus_size"] = local_constants::focus_group::max_focus_group_size;

//     return summary;
// }

// Task<> Sequence::completeFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
//     Json::Value focus_group_summary;

//     try {
//         auto users_db = app().getDbClient(local_constants::users_db);
//         auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

//         // get focus group && summary
//         auto focus_group_and_sequence_result = co_await users_db->execSqlCoro(
//             "select focus_group, to_learn_words from user_sequence where user_id = $1", user_id
//         );
//         auto focus_group_str = focus_group_and_sequence_result[0]["focus_group"].as<std::string>();

//         Json::Value focus_group;

//         if (!Json::Reader().parse(focus_group_str, focus_group)) {
//             throw std::runtime_error("failed_to_parse_focus_group");
//         }

//         if (!focus_group.isMember("words")) {
//             co_return local_utils::Response::Error::ERROR(callback, k204NoContent, "");
//         }

//         focus_group_summary = get_focus_group_summary(focus_group);

//         if (focus_group_summary.isMember("words_phases_mistakes") && focus_group_summary["words_phases_mistakes"].empty()) {
//             co_return local_utils::Response::Error::ERROR(callback, k204NoContent, "");
//         }

//         const auto& summary_field = focus_group_summary["summary"];
//         auto current_focus_group_size = summary_field["current_focus_size"].as<uint32_t>();
//         auto new_focus_group_size = summary_field["new_focus_size"].as<uint32_t>();
//         auto learned_words_count = summary_field["learned"].as<uint32_t>();
//         auto repeated_words_count = summary_field["repeated"].as<uint32_t>();
//         auto failed_words_count = summary_field["failed"].as<uint32_t>();

//         auto focus_group_summary_str = local_utils::json::json_to_string(focus_group_summary);

//         // create empty focus group
//         Json::Value empty_focus_group;
//         auto empty_focus_group_str = local_utils::json::json_to_string(empty_focus_group);

//         auto current_time = local_utils::time::get_unix_timestamp();

//         // update user words sequence && create user words to append
//         auto to_learn_words_str = focus_group_and_sequence_result[0]["to_learn_words"].as<std::string>();
//         auto to_learn_words_vec = core::user_sequence_words::sequence_user_words_str_to_vector(to_learn_words_str);

//         std::unordered_map<std::string, std::string> user_words_to_attach_by_src_lang;
//         std::unordered_map<std::string, std::unordered_map<std::string, uint8_t>> user_words_info_by_src_lang;

//         auto& words = focus_group["words"];

//         for (auto& word : words) {
//             const auto& phases_mistakes = word["phases_mistakes"];

//             bool word_has_mistake = false;

//             for (const auto &phase_mistake : phases_mistakes) {
//                 if (phase_mistake.asBool()) {
//                     word_has_mistake = true;
//                 }
//             }

//             auto current_phase = word["current_phase"].as<uint32_t>();
//             auto confirmation_level = word["confirmation_level"].as<uint32_t>();

//             if (word_has_mistake) {
//                 confirmation_level = 0;
//             } else if (current_phase != 0) {
//                 ++confirmation_level;
//             }

//             auto word_str = word["word"].as<std::string>();
//             auto src_lang = word["src_lang"].as<std::string>();

//             auto user_word_str = core::progress_words::create_user_word_str(word_str, false, confirmation_level, current_time);

//             if (user_words_to_attach_by_src_lang.contains(src_lang)) {
//                 user_words_to_attach_by_src_lang.at(src_lang) += user_word_str;
//             } else {
//                 user_words_to_attach_by_src_lang[src_lang] = user_word_str;
//             }

//             user_words_info_by_src_lang[src_lang][word_str] = confirmation_level;
//         }

//         // update to_learn_words if exists
//         for (auto& to_learn_word : to_learn_words_vec) {
//             if (user_words_info_by_src_lang.contains(to_learn_word.src_lang) 
//                 && user_words_info_by_src_lang[to_learn_word.src_lang].contains(to_learn_word.word)
//             ) {
//                 auto new_confirmation_level = user_words_info_by_src_lang.at(to_learn_word.src_lang).at(to_learn_word.word);

//                 if (new_confirmation_level > to_learn_word.confirmation_level) {
//                     to_learn_word.is_passed = true;
//                 }

//                 to_learn_word.confirmation_level = new_confirmation_level;
//             }
//         }

//         auto to_learn_words_updated_str = core::user_sequence_words::sequence_user_words_to_str(to_learn_words_vec);

//         // append user words by src_lang
//         for (const auto& user_words_to_attach : user_words_to_attach_by_src_lang) {
//             auto user_words_update_result = co_await users_db->execSqlCoro(
//                 "UPDATE user_words SET words = words || $1, last_update_id = last_update_id + 1 WHERE user_id = $2 AND src_lang = $3", 
//                 user_words_to_attach.second, user_id, user_words_to_attach.first
//             );

//             if (!user_words_update_result.affectedRows()) {
//                 throw std::runtime_error("user_words_not_updated");
//             }
//         }

//         // update focus_size, to_learn_words (for words with not mistakes), update previous_focus_group_result with summary
//         auto update_user_sequence_result = co_await users_db->execSqlCoro(
//             "update user_sequence set focus_group_size = $1, focus_group = $2, to_learn_words = $3, focus_group_last_update_time = $4,"
//             " previous_focus_group_result = $5 where user_id = $6", new_focus_group_size, empty_focus_group_str, to_learn_words_updated_str, 
//             current_time, focus_group_summary_str, user_id
//         );

//         if (!update_user_sequence_result.affectedRows()) {
//             throw std::runtime_error("user_sequence_not_updated");
//         }

//         // update statistics
//         auto statistics_info_result = co_await users_db->execSqlCoro(
//             "select learned_this_time, last_learn_time, learned_today, learned_words_total, learn_streak, total_learn_days, last_repeat_time,"
//             " repeated_today, focus_size_total_sum, focus_size_changes_total_count from user_statistics where user_id = $1", user_id
//         );

//         auto learned_this_time = statistics_info_result[0]["learned_this_time"].as<uint32_t>();
//         auto last_learn_time = statistics_info_result[0]["last_learn_time"].as<uint32_t>();
//         auto learned_today = statistics_info_result[0]["learned_today"].as<uint32_t>();
//         auto learned_words_total = statistics_info_result[0]["learned_words_total"].as<int32_t>();
//         auto learn_streak = statistics_info_result[0]["learn_streak"].as<uint32_t>();
//         auto total_learn_days = statistics_info_result[0]["total_learn_days"].as<uint32_t>();
//         auto last_repeat_time = statistics_info_result[0]["last_repeat_time"].as<uint32_t>();
//         auto repeated_today = statistics_info_result[0]["repeated_today"].as<uint32_t>();
//         auto focus_size_total_sum = statistics_info_result[0]["focus_size_total_sum"].as<uint64_t>();
//         auto focus_size_changes_total_count = statistics_info_result[0]["focus_size_changes_total_count"].as<uint32_t>();

//         auto days_since_current_time = local_utils::time::get_days_passed_since_epoch(current_time);
//         auto days_since_last_learn_time = local_utils::time::get_days_passed_since_epoch(last_learn_time);
//         auto days_since_last_repeat_time = local_utils::time::get_days_passed_since_epoch(last_repeat_time);

//         if (current_time - last_learn_time > local_constants::statistics::learned_this_time_expire_seconds) {
//             learned_this_time = learned_words_count + repeated_words_count;
//         } else {
//             learned_this_time += learned_words_count + repeated_words_count;
//         }

//         if (days_since_current_time != days_since_last_learn_time) {
//             learned_today = learned_words_count;
//         } else {
//             learned_today += learned_words_count;
//         }

//         learned_words_total += static_cast<int32_t>(learned_words_count) - failed_words_count;

//         if (learned_words_count < 0) {
//             learned_words_count = 0;
//         }

//         if (days_since_current_time != days_since_last_repeat_time) {
//             repeated_today = repeated_words_count;
//         } else {
//             repeated_today += repeated_words_count;
//         }

//         auto days_difference_since_last_learn = days_since_current_time - days_since_last_learn_time;
//         if (days_difference_since_last_learn == 1) {
//             learn_streak += 1;
//         } else if (days_difference_since_last_learn > 1) {
//             learn_streak = 0;
//         }

//         if (days_difference_since_last_learn > 0) {
//             total_learn_days += 1;
//         }

//         if (current_focus_group_size != new_focus_group_size) {
//             focus_size_total_sum += new_focus_group_size;
//             focus_size_changes_total_count += 1;
//         }

//         last_learn_time = current_time;
//         last_repeat_time = current_time;

//         auto update_user_statistics_result = co_await users_db->execSqlCoro(" update user_statistics"
//             " set learned_this_time = $1,"
//             " last_learn_time = $2,"
//             " learned_today = $3,"
//             " learned_words_total = $4,"
//             " learn_streak = $5,"
//             " total_learn_days = $6,"
//             " last_repeat_time = $7,"
//             " repeated_today = $8,"
//             " focus_size_total_sum = $9,"
//             " focus_size_changes_total_count = $10"
//             " where user_id = $11", 
//             learned_this_time, last_learn_time, learned_today, learned_words_total, learn_streak, total_learn_days, 
//             last_repeat_time, repeated_today, focus_size_total_sum, focus_size_changes_total_count, user_id
//         );

//         if (!update_user_statistics_result.affectedRows()) {
//             throw std::runtime_error("user statistics not updated");
//         }

//         // update generation limits
//         co_await increase_generated_phrases_count(users_db, user_id, learned_words_count + repeated_words_count);
//     } catch (const std::exception& e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Response::Error::SERVER_ERROR(callback);
//     }
//     co_return local_utils::Response::JSON_OK(callback, focus_group_summary);
// }

// Task<uint32_t> Sequence::get_learned_this_time(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
//     auto statistics_info_result = co_await users_db->execSqlCoro(
//         "select learned_this_time, last_learn_time from user_statistics where user_id = $1", user_id
//     );
//     auto learned_this_time = statistics_info_result[0]["learned_this_time"].as<uint32_t>();
// 	auto last_learn_time = statistics_info_result[0]["last_learn_time"].as<uint32_t>();

//     auto current_time = local_utils::time::get_unix_timestamp();

//     co_return (current_time - last_learn_time > local_constants::statistics::learned_this_time_expire_seconds) ? 0 : learned_this_time;
// }

// Task<bool> Sequence::get_is_multiple_languages(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
//     auto languages_count_result = co_await users_db->execSqlCoro(
//         "select user_id, COUNT(DISTINCT src_lang) AS unique_src_lang_count"
//         " from user_language_pairs where is_enabled = true and user_id = $1 GROUP BY user_id", user_id
//     );
//     co_return languages_count_result[0]["unique_src_lang_count"].as<uint32_t>() > 1;
// }
}
#include "core/user_sequence/user_item.h"
#include "utils/utils.h"
#include "core/user_sequence_words/user_sequence_words.h"
#include "core/text_processing/text_processor.h"

namespace core::user_sequence {

std::string encode_item_id(uint32_t group_id, uint16_t item_id, ItemType item_type) {
    return std::to_string(group_id) + '.' + std::to_string(item_id) + '.' + std::to_string(static_cast<uint16_t>(item_type));
}

std::tuple<uint32_t, uint16_t, ItemType> decode_item_id(const std::string& item_id_str) {
    std::stringstream ss(item_id_str);
    uint32_t group_id;
    uint16_t item_id, item_type;
    char dot1, dot2;

    ss >> group_id >> dot1 >> item_id >> dot2 >> item_type;

    if (dot1 != '.' || dot2 != '.') {
        throw std::invalid_argument("can not decode item id");
    }

    return {group_id, item_id, static_cast<ItemType>(item_type)};
}


drogon::Task<uint32_t> insert_user_items(drogon::orm::DbClientPtr users_db, uint64_t user_id, 
    const std::vector<core::phrases_generator::FocusGroupPhrase>& group_phrases, bool add_rate_us
) {
    auto group_update_result = co_await users_db->execSqlCoro(
        "update user_group set group_id = group_id + 1 where user_id = $1 returning group_id", user_id
    );

    if (group_update_result.empty()) {
        throw std::runtime_error("group_not_updated");

    }

    auto new_group_id = group_update_result[0]["group_id"].as<uint32_t>();

    auto time_now = local_utils::time::get_unix_timestamp();

    std::string group_items_insert_query = R"(
        INSERT INTO user_items (user_id, group_id, item_id, created_at, item_type, src_lang, dst_lang, word)
        VALUES
    )";

    uint32_t item_id = 0;
    uint32_t current_item_id = 0;
    for (uint8_t phase_num = 0; phase_num < local_constants::focus_group::phases_per_item; ++phase_num) {
        for (const auto& group_phrase : group_phrases) {
            ItemType item_type;

            if (phase_num == 0) item_type = ItemType::first;
            else if (phase_num == 1) item_type = ItemType::second;
            else if (phase_num == 2) item_type = ItemType::third;

            current_item_id = item_id++;

            if (current_item_id > 0) group_items_insert_query += ",\n";

            group_items_insert_query += "(" + std::to_string(user_id)
                + ',' + std::to_string(new_group_id)
                + ',' + std::to_string(current_item_id)
                + ',' + std::to_string(time_now)
                + ',' + std::to_string(static_cast<uint16_t>(item_type))
                + ",'" + group_phrase.generated_phrase.src_lang + "'"
                + ",'" + group_phrase.generated_phrase.dst_lang + "'"
                + ",'" + group_phrase.generated_phrase.src_word + "'"
                + ")";
        }
    }

    if (add_rate_us) {
        current_item_id = item_id++;
        // add rate_us item
        group_items_insert_query += ",\n("
        + std::to_string(user_id)
        + ',' + std::to_string(new_group_id)
        + ',' + std::to_string(current_item_id)
        + ',' + std::to_string(time_now)
        + ',' + std::to_string(static_cast<uint16_t>(ItemType::rate_us))
        + ", NULL"
        + ", NULL"
        + ", NULL"
        + ")";
    }

    current_item_id = item_id++;
    // add result item
    group_items_insert_query += ",\n("
    + std::to_string(user_id)
    + ',' + std::to_string(new_group_id)
    + ',' + std::to_string(current_item_id)
    + ',' + std::to_string(time_now)
    + ',' + std::to_string(static_cast<uint16_t>(ItemType::result))
    + ", NULL"
    + ", NULL"
    + ", NULL"
    + ")";

    group_items_insert_query += ';';

    auto items_insert_result = co_await users_db->execSqlCoro(group_items_insert_query);

    if (items_insert_result.affectedRows() == 0) {
        throw std::runtime_error("failed_to_insert_user_items");
    }

    co_return new_group_id;
}

drogon::Task<std::optional<UserItem>> get_current_user_item(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto result = co_await users_db->execSqlCoro(
        " SELECT"
            " ui.group_id,"
            " ui.item_id,"
            " ui.is_passed,"
            " ui.is_hidden,"
            " ui.created_at,"
            " ui.passed_at,"
            " ui.item_type,"
            " ui.src_lang,"
            " ui.dst_lang,"
            " ui.word,"
            " ui.mistakes_count,"
            " ui.seconds_spent,"
            " ui.seconds_spent_on_answer"
        " FROM user_items ui"
        " WHERE ui.user_id = $1"
            " AND ui.group_id = ("
                " SELECT ug.group_id"
                " FROM user_group ug"
                " WHERE ug.user_id = ui.user_id"
            " )"
            " AND ui.is_hidden = FALSE"
            " AND ui.is_passed = FALSE"
        " ORDER BY ui.item_id"
        " LIMIT 1", user_id
    );

    if (result.empty()) {
        co_return std::nullopt;
    }

    auto group_id = result[0]["group_id"].as<uint32_t>();
    auto item_id = result[0]["item_id"].as<uint16_t>();
    auto is_passed = result[0]["is_passed"].as<bool>();
    auto is_hidden = result[0]["is_hidden"].as<bool>();
    auto created_at = result[0]["created_at"].as<uint32_t>();

    uint32_t passed_at = result[0]["passed_at"].isNull() ? 0 : result[0]["passed_at"].as<uint32_t>();

    auto item_type = static_cast<ItemType>(result[0]["item_type"].as<uint16_t>());

    std::string src_lang = result[0]["src_lang"].isNull() ? "" : result[0]["src_lang"].as<std::string>();
    std::string dst_lang = result[0]["dst_lang"].isNull() ? "" : result[0]["dst_lang"].as<std::string>();
    std::string word = result[0]["word"].isNull() ? "" : result[0]["word"].as<std::string>();

    uint16_t mistakes_count = result[0]["mistakes_count"].isNull() ? 0 : result[0]["mistakes_count"].as<uint16_t>();
    uint16_t seconds_spent = result[0]["seconds_spent"].isNull() ? 0 : result[0]["seconds_spent"].as<uint16_t>();
    uint16_t seconds_spent_on_answer = result[0]["seconds_spent_on_answer"].isNull() ? 0 : result[0]["seconds_spent_on_answer"].as<uint16_t>();

    co_return UserItem(
        group_id,
        item_id,
        is_passed,
        is_hidden,
        created_at,
        passed_at,
        item_type,
        src_lang,
        dst_lang,
        word,
        mistakes_count,
        seconds_spent,
        seconds_spent_on_answer
    );
}

// Json::Value replace_words_with_null(const std::string& phrase, const std::string& words, const std::string& lang) {
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

Json::Value replace_words_with_null(const std::string& phrase, const std::string& words, const std::string& lang) {
    auto phrase_copy = phrase;

    std::string special_str = "696969";
    std::string whitespace = " ";
    std::string comma = ",";

    auto capitalized_words = local_utils::strings::get_capitalized_string(words);
    local_utils::strings::replace_all(phrase_copy, capitalized_words, special_str);
    auto lower_words = core::text_processing::unicode_str_to_lower(words, lang.c_str());
    local_utils::strings::replace_all(phrase_copy, lower_words, special_str);

    auto phrase_parts = core::text_processing::extract_words_and_punctuation_from_str(phrase_copy, lang.c_str());

    Json::Value json_parts = Json::arrayValue;

    std::string comma_part;
    for (const auto& part : phrase_parts) {
        if (part == special_str) {
            json_parts.append(Json::nullValue);
        } else if (part == whitespace) {
            continue;
        } else {
            if (part == comma) {
                comma_part += ", ";
                continue;
            } else if (comma_part.size()) {
                json_parts.append(comma_part + part);
                comma_part.clear();
            } else {
                json_parts.append(part);
            }
        }
    }

    return json_parts;
}

drogon::Task<core::phrases_generator::FocusGroupPhrase> generate_focus_group_phrase(const std::string& src_lang, const std::string& dst_lang, const std::string& word) {
    auto openai_phrase_generation = drogon::app().getPlugin<OpenaiGeneration>();
    auto frequent_words_plugin = drogon::app().getPlugin<FrequentWords>();
    auto translations_db = drogon::app().getDbClient(local_constants::translations_db);
    core::phrases_generator::PhrasesGenerator phrases_generator(openai_phrase_generation, frequent_words_plugin, translations_db);

    auto user_word = core::user_sequence_words::SequenceUserWord(src_lang, dst_lang, word, 0, false);
    auto generated_phrase = co_await phrases_generator.generate_phrase_with_words_and_letters(
        user_word,
        false
    );

    co_return generated_phrase;
}

void add_phrases_and_placeholders_to_content(Json::Value &content, const core::phrases_generator::FocusGroupPhrase& generated_phrase) {
    content["src_placeholder"] = generated_phrase.generated_phrase.src_word;
    content["dst_placeholder"] = generated_phrase.generated_phrase.dst_word;
    content["src_phrase"] = replace_words_with_null(generated_phrase.generated_phrase.src_phrase, generated_phrase.generated_phrase.src_word, generated_phrase.generated_phrase.src_lang);
    content["dst_phrase"] = replace_words_with_null(generated_phrase.generated_phrase.dst_phrase, generated_phrase.generated_phrase.dst_word, generated_phrase.generated_phrase.dst_lang);
}

drogon::Task<Json::Value> UserItem::create_json_user_item(const Json::Value& group_result_json) const {
    Json::Value result;

    result["type"] = to_string(type);
    result["id"] = encode_item_id(group_id, id, type);

    if (type == ItemType::first) {
        auto& content = result["content"] = Json::objectValue;
        auto generated_phrase = co_await generate_focus_group_phrase(src_lang, dst_lang, word);
        add_phrases_and_placeholders_to_content(content, generated_phrase);
        co_return result;
    } else if (type == ItemType::second) {
        auto& content = result["content"] = Json::objectValue;
        auto generated_phrase = co_await generate_focus_group_phrase(src_lang, dst_lang, word);
        add_phrases_and_placeholders_to_content(content, generated_phrase);

        auto& variants = content["variants"] = Json::arrayValue;
        for (const auto& variant : generated_phrase.guess_words) {
            variants.append(std::move(variant));
        }

        co_return result;
    } else if (type == ItemType::third) {
        auto& content = result["content"] = Json::objectValue;
        auto generated_phrase = co_await generate_focus_group_phrase(src_lang, dst_lang, word);
        add_phrases_and_placeholders_to_content(content, generated_phrase);

        auto& variants = content["variants"] = Json::arrayValue;
        for (const auto& variant : generated_phrase.guess_letters) {
            variants.append(std::move(variant));
        }

        co_return result;
    } else if (type == ItemType::result) {
        auto& results = result["results"] = group_result_json != Json::nullValue ? group_result_json : Json::arrayValue;
        co_return result;
    } else if (type == ItemType::rate_us) {
        co_return result;
    } else {
        throw std::runtime_error("unknown user item type");
    }
}
}
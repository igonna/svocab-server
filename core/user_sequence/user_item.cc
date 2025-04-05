#include "core/user_sequence/user_item.h"
#include "utils/utils.h"

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

drogon::Task<std::optional<UserItem>> UserItem::get_current_user_item(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
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

drogon::Task<Json::Value> UserItem::create_user_item_json() const {
    Json::Value result;

    result["type"] = to_string(type);
    result["id"] = encode_item_id(group_id, id, type);

    if (type == ItemType::first) {

    } else if (type == ItemType::second) {

    } else if (type == ItemType::third) {

    } else if (type == ItemType::result) {
        // TODO: what if we need to get additional data for result?
    } else if (type == ItemType::rate_us) {

    }
    

    co_return result;
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
}

#pragma once

#include <drogon/drogon.h>
#include <drogon/utils/coroutine.h>
#include <vector>
#include <cstdint>
#include "core/user_sequence_words/user_sequence_words.h"
#include "core/phrases_generation/phrases_generator.h"

namespace core::user_sequence {
    enum class ItemType : uint16_t {first, second, third, result, rate_us};

    inline std::string to_string(ItemType type) {
        switch (type) {
            case ItemType::first:   return "first";
            case ItemType::second:  return "second";
            case ItemType::third:   return "third";
            case ItemType::result:  return "result";
            case ItemType::rate_us: return "rate_us";
            default:                throw std::invalid_argument("Unknown ItemType");
        }
    }

    struct UserItem {        
        drogon::Task<Json::Value> create_json_user_item(const Json::Value& group_result_json = Json::nullValue) const;

        UserItem(
            UserItem&& other
        ) : group_id(other.group_id),
            id(other.id),
            is_passed(other.is_passed),
            is_hidden(other.is_hidden),
            created_at(other.created_at),
            passed_at(other.passed_at),
            type(other.type),
            src_lang(std::move(other.src_lang)),
            dst_lang(std::move(other.dst_lang)),
            word(std::move(other.word)),
            mistakes_count(other.mistakes_count),
            seconds_spent(other.seconds_spent),
            seconds_spent_on_answer(other.seconds_spent_on_answer) {}

        UserItem& operator=(UserItem&& other) noexcept {
            if (this != &other) {
                group_id = other.group_id;
                id = other.id;
                is_passed = other.is_passed;
                is_hidden = other.is_hidden;
                created_at = other.created_at;
                passed_at = other.passed_at;
                type = other.type;
                src_lang = std::move(other.src_lang);
                dst_lang = std::move(other.dst_lang);
                word = std::move(other.word);
                mistakes_count = other.mistakes_count;
                seconds_spent = other.seconds_spent;
                seconds_spent_on_answer = other.seconds_spent_on_answer;
            }
            return *this;
        }

        UserItem(
            uint32_t group_id,
            uint16_t id,
            bool is_passed,
            bool is_hidden,
            uint32_t created_at,
            uint32_t passed_at,
            ItemType type,
            const std::string& src_lang,
            const std::string& dst_lang,
            const std::string& word,
            uint16_t mistakes_count,
            uint16_t seconds_spent,
            uint16_t seconds_spent_on_answer
        ) : group_id(group_id),
            id(id),
            is_passed(is_passed),
            is_hidden(is_hidden),
            created_at(created_at),
            passed_at(passed_at),
            type(type),
            src_lang(src_lang),
            dst_lang(dst_lang),
            word(word),
            mistakes_count(mistakes_count),
            seconds_spent(seconds_spent),
            seconds_spent_on_answer(seconds_spent_on_answer) {}

        uint32_t group_id;
        uint16_t id;
        bool is_passed;
        bool is_hidden;
        uint32_t created_at;
        uint32_t passed_at;

        ItemType type;

        std::string src_lang;
        std::string dst_lang;
        std::string word;

        uint16_t mistakes_count;
        uint16_t seconds_spent;
        uint16_t seconds_spent_on_answer;
    };

    std::string encode_item_id(uint32_t group_id, uint16_t item_id, ItemType item_type);
    std::tuple<uint32_t, uint16_t, ItemType> decode_item_id(const std::string& item_id_str);

    // return new group_id
    drogon::Task<uint32_t> insert_user_items(drogon::orm::DbClientPtr users_db, uint64_t user_id, 
        const std::vector<core::phrases_generator::FocusGroupPhrase>& group_phrases, bool add_rate_us
    );

    Json::Value replace_words_with_null(const std::string& phrase, const std::string& words, const std::string& lang);

    drogon::Task<std::optional<UserItem>> get_current_user_item(drogon::orm::DbClientPtr users_db, uint64_t user_id);
}
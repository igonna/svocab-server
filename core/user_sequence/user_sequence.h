#pragma once

#include <drogon/drogon.h>
#include <drogon/utils/coroutine.h>
#include <vector>
#include <cstdint>
#include "core/user_sequence_words/user_sequence_words.h"

namespace core::user_sequence {

    struct SequenceInfo {
        SequenceInfo(
            SequenceInfo&& other
        ) : is_sequence_generation_required(other.is_sequence_generation_required),
            sequence_last_update_id(other.sequence_last_update_id),
            to_learn_words(std::move(other.to_learn_words)),
            is_focus_group_generation_required(other.is_focus_group_generation_required),
            focus_group_id(other.focus_group_id),
            focus_group_size(other.focus_group_size),
            focus_group(std::move(other.focus_group)) {}

        SequenceInfo& operator=(SequenceInfo&& other) noexcept {
            if (this != &other) {
                is_sequence_generation_required = other.is_sequence_generation_required;
                sequence_last_update_id = other.sequence_last_update_id;
                to_learn_words = std::move(other.to_learn_words);
                is_focus_group_generation_required = other.is_focus_group_generation_required;
                focus_group_id = other.focus_group_id;
                focus_group_size = other.focus_group_size;
                focus_group = std::move(other.focus_group);
            }
            return *this;
        }

        SequenceInfo(
            bool is_sequence_generation_required,
            uint32_t sequence_last_update_id,
            std::string&& to_learn_words,
            bool is_focus_group_generation_required,
            uint32_t focus_group_id,
            uint32_t focus_group_size,
            Json::Value&& focus_group
        ) : is_sequence_generation_required(is_sequence_generation_required),
            sequence_last_update_id(sequence_last_update_id),
            to_learn_words(std::move(to_learn_words)),
            is_focus_group_generation_required(is_focus_group_generation_required),
            focus_group_id(focus_group_id),
            focus_group_size(focus_group_size),
            focus_group(std::move(focus_group)) {}

        bool is_sequence_generation_required;
        uint32_t sequence_last_update_id;
        std::string to_learn_words;
        bool is_focus_group_generation_required;
        uint32_t focus_group_id;
        uint32_t focus_group_size;
        Json::Value focus_group;
    };

    drogon::Task<SequenceInfo> get_sequence_info(drogon::orm::DbClientPtr users_db, uint64_t user_id);
    uint32_t get_topic_mix_value(uint32_t topic_index, uint32_t created_at, uint32_t current_time);
    drogon::Task<std::vector<core::user_sequence_words::SequenceUserWord>> get_updated_user_sequence(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool is_sequence_generation_required, uint32_t sequence_last_update_id, const std::string& user_words_str);
    drogon::Task<Json::Value> get_updated_focus_group(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool is_focus_group_generation_required, std::vector<core::user_sequence_words::SequenceUserWord>& sequence_words);
}
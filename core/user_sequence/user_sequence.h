#pragma once

#include <drogon/drogon.h>
#include <drogon/utils/coroutine.h>
#include <vector>
#include <cstdint>
#include "core/user_sequence_words/user_sequence_words.h"
#include "core/phrases_generation/phrases_generator.h"

namespace core::user_sequence {
    uint32_t get_topic_mix_value(uint32_t topic_index, uint32_t created_at, uint32_t current_time);
    drogon::Task<std::vector<core::user_sequence_words::SequenceUserWord>> get_updated_user_sequence(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t sequence_last_update_id);
    // drogon::Task<> core::user_sequence::process_item_request(drogon::orm::DbClientPtr users_db, uint64_t user_id, const Json::Value& item_request);

    drogon::Task<std::vector<core::phrases_generator::FocusGroupPhrase>> generate_group_phrases(
        drogon::orm::DbClientPtr users_db,
        uint32_t user_id,
        bool user_has_subscription,
        std::vector<core::user_sequence_words::SequenceUserWord>& user_words_sequence,
        uint32_t group_size
    );
}
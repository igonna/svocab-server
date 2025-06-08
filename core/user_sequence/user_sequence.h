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
        uint64_t user_id,
        bool user_has_subscription,
        std::vector<core::user_sequence_words::SequenceUserWord>& user_words_sequence,
        uint32_t group_size
    );


    enum class OfferType : uint16_t {rate_us};

    drogon::Task<bool> is_user_got_offer(drogon::orm::DbClientPtr users_db, uint64_t user_id, OfferType offer_type);
    drogon::Task<> set_user_got_offer(drogon::orm::DbClientPtr users_db, uint64_t user_id, OfferType offer_type, uint16_t user_choice);
    drogon::Task<bool> should_user_get_rate_us_offer(drogon::orm::DbClientPtr users_db, uint64_t user_id);


    drogon::Task<> set_learn_item_passed(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t group_id,
        uint16_t item_id, uint16_t mistakes, uint16_t seconds_spent, uint16_t seconds_spent_on_answer
    );

    // src_lang, word
    drogon::Task<std::tuple<std::string, std::string>> set_learn_item_hidden(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t group_id,
        uint16_t item_id, uint16_t seconds_spent
    );

    drogon::Task<> set_user_word_hidden(drogon::orm::DbClientPtr users_db, uint64_t user_id,
        const std::string& src_lang, const std::string& word
    );

    drogon::Task<> set_user_item_passed(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t 
        group_id, uint16_t item_id, uint16_t seconds_spent
    );

    drogon::Task<> update_group_last_interaction_time(drogon::orm::DbClientPtr users_db, uint64_t user_id);

    // status_code, message, did_user_hide_item
    drogon::Task<std::optional<std::tuple<drogon::HttpStatusCode, std::string, bool>>> update_item_progress(
        drogon::orm::DbClientPtr users_db, uint64_t user_id, const Json::Value &item_request
    );

    drogon::Task<bool> is_user_blocked_from_generation(drogon::nosql::RedisClientPtr redis_client, uint64_t user_id);
    drogon::Task<> block_user_from_generation(drogon::nosql::RedisClientPtr redis_client, uint64_t user_id);

    Json::Value create_result_values_json(std::string type, const std::vector<uint32_t> values, const std::vector<std::string>& words);
    // group_result, group_size_updated
    drogon::Task<std::tuple<Json::Value, bool>> update_user_words_with_current_group(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t group_id);
}
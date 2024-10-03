#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <string>
#include <vector>
#include "core/topic_generation/topic_generator.h"
#include "core/text_processing/text_processor.h"


namespace core::sql {

static drogon::Task<> update_user_sequence_last_update_id(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto result = co_await users_db->execSqlCoro("update user_sequence set last_update_id = last_update_id + 1 where user_id = $1", user_id);
    if (!result.affectedRows()) {
        throw std::runtime_error("user_sequence_not_updated");
    }
    co_return;
}

static drogon::Task<std::string> get_user_topic_src_lang(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id) {
    auto result = co_await users_db->execSqlCoro("select src_lang from user_topics where user_id = $1 and topic_id = $2", user_id, topic_id);
    if (!result.size()) {
        throw std::runtime_error("topic_not_exists");
    }
    co_return result[0]["src_lang"].as<std::string>();
}

static drogon::Task<uint32_t> get_topic_new_source_id(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id) {
    auto result = co_await users_db->execSqlCoro("update user_topics "
        "set last_source_id = last_source_id + 1 where user_id = $1 and topic_id = $2 returning last_source_id", 
        user_id, topic_id
    );

    if (!result.affectedRows()) {
        throw std::runtime_error("topic_not_exists");
    }

    co_return result[0]["last_source_id"].as<uint32_t>();
}

static drogon::Task<uint32_t> add_topic_source(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id, const std::string& title, 
    const std::string& type, const std::string& processed_words, const std::string& text = "", const std::string& fail_type = "") 
{
    auto new_topic_source_id = co_await get_topic_new_source_id(users_db, user_id, topic_id);
    auto new_path = std::string("{") + std::to_string(new_topic_source_id) + "}";

    Json::Value metadata;
    metadata["title"] = title;
    metadata["type"] = type;
    if (fail_type.size()) {
        metadata["fail_type"] = fail_type;
    }

    auto result = co_await users_db->execSqlCoro(
        "update user_topics set "
        "sources_metadata = jsonb_set(sources_metadata, $3, $4), "
        "sources_unprocessed = jsonb_set(sources_unprocessed, $3, to_jsonb($5::text)), "
        "sources_processed = jsonb_set(sources_processed, $3, to_jsonb($6::text)), "
        "last_update_id = last_update_id + 1 "
        "where user_id = $1 and topic_id = $2"
        , user_id, topic_id, new_path, metadata.toStyledString(), text, processed_words
    );

    if (!result.affectedRows()) {
        throw std::runtime_error("can_not_update");
    }

    co_return new_topic_source_id;
}

static drogon::Task<> update_topic_source(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id, uint32_t source_id, const std::string& title, 
    const std::string& type, const std::string& processed_words, const std::string& text = "", const std::string& fail_type = "") 
{
    auto path = std::string("{") + std::to_string(source_id) + "}";

    Json::Value metadata;
    metadata["title"] = title;
    metadata["type"] = type;
    if (fail_type.size()) {
        metadata["fail_type"] = fail_type;
    }

    auto result = co_await users_db->execSqlCoro(
        "update user_topics set "
        "sources_metadata = jsonb_set(sources_metadata, $3, $4), "
        "sources_unprocessed = jsonb_set(sources_unprocessed, $3, to_jsonb($5::text)), "
        "sources_processed = jsonb_set(sources_processed, $3, to_jsonb($6::text)), "
        "last_update_id = last_update_id + 1 "
        "where user_id = $1 and topic_id = $2"
        , user_id, topic_id, path, metadata.toStyledString(), text, processed_words
    );

    if (!result.affectedRows()) {
        throw std::runtime_error("can_not_update");
    }
}

// generate topic sequence and update 
static Task<std::string> generate_topic_sequence_and_if_update(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id, uint32_t topic_last_update_id) {
    std::string result_sequence;

    // fetch topic gen data
    auto topic_sources = co_await users_db->execSqlCoro(
        "select src_lang, skip_value, order_words_by, "
        "exclude_word_if_not_present_in_all_sources, exclude_word_if_very_rare, exclude_word_if_word_learned, "
        "key, value from user_topics, jsonb_each_text(sources_processed) "
        "where user_id = $1 and topic_id = $2",
        user_id, topic_id
    );

    if (topic_sources.empty()) {
        co_return result_sequence;
    }

    auto src_lang = topic_sources[0]["src_lang"].as<std::string>();
    auto skip_value = topic_sources[0]["skip_value"].as<float>();
    auto order_words_by = topic_sources[0]["order_words_by"].as<uint16_t>();
    auto exclude_word_if_not_present_in_all_sources = topic_sources[0]["exclude_word_if_not_present_in_all_sources"].as<bool>();
    auto exclude_word_if_very_rare = topic_sources[0]["exclude_word_if_very_rare"].as<bool>();
    auto exclude_word_if_word_learned = topic_sources[0]["exclude_word_if_word_learned"].as<bool>(); // TODO

    auto topic_generator = core::topic_generation::TopicGenerator(
        src_lang,
        skip_value,
        static_cast<core::topic_generation::TopicGenerator::OrderWordsBy>(order_words_by),
        exclude_word_if_not_present_in_all_sources,
        exclude_word_if_very_rare,
        exclude_word_if_word_learned
    );

    for (auto& source : topic_sources) {
        auto source_processed_words = source["value"].as<std::string>();
        auto words = core::text_processing::processed_words_str_to_binary(source_processed_words);
        topic_generator.add_topic_words(words);
    }

    auto new_sequence = topic_generator.get_topic_sequence_as_string();

    try {
        auto topic_sequence_update_result = co_await users_db->execSqlCoro(
            "update user_topics set last_gen_id = $1, topic_words_sequence = $2 "
            "where user_id = $3 and topic_id = $4 and last_update_id = $1", 
            topic_last_update_id, new_sequence, user_id, topic_id
        );
    } catch (const std::exception& e) {
    }

    co_return new_sequence;
}

static Task<bool> is_user_of_sepcial_user_type(drogon::orm::DbClientPtr users_db, uint64_t user_id, std::string special_user_type) {
    auto result = co_await users_db->execSqlCoro("select 1 from users where id = $1 and "
        "special_user_type = '" + special_user_type + "'::SpecialUserType limit 1", user_id
    );

    co_return result.size();
}

static Task<std::string> get_topic_words_sequence_and_if_update(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id) {
    // check if generation required
    auto result_update_required = co_await users_db->execSqlCoro(
        "select last_update_id from user_topics "
        "where user_id = $1 and topic_id = $2 and last_gen_id != last_update_id limit 1", 
        user_id, topic_id
    );

    std::string topic_words_sequence;
    if (result_update_required.size()) {
        auto topic_last_update_id = result_update_required[0]["last_update_id"].as<uint32_t>();
        topic_words_sequence = co_await core::sql::generate_topic_sequence_and_if_update(users_db, user_id, topic_id, topic_last_update_id);
    } else {
        auto result = co_await users_db->execSqlCoro("select topic_words_sequence from user_topics "
            "where user_id = $1 and topic_id = $2", user_id, topic_id
        );

        if (!result.size()) {
            throw std::runtime_error("not_exists");
        }

        topic_words_sequence = result[0]["topic_words_sequence"].as<std::string>();
    }

    co_return topic_words_sequence;
}

static Task<int32_t> get_new_smallest_topic_position(drogon::orm::DbClientPtr users_db, uint64_t user_id, const std::string& src_lang, const std::string& dst_lang) {
    auto result_position = co_await users_db->execSqlCoro("SELECT COALESCE(MIN(position), 0) AS smallest_position "
        "FROM (SELECT position FROM user_topics WHERE user_id = $1 AND src_lang = $2 AND dst_lang = $3 AND is_enabled = TRUE "
        "UNION ALL "
        "SELECT position FROM user_global_topics "
        "WHERE user_id = $1 AND src_lang = $2 AND dst_lang = $3 AND is_enabled = TRUE) AS combined_positions", user_id, src_lang, dst_lang
    );

    co_return result_position[0]["smallest_position"].as<int32_t>() - 1;
}

static Task<int32_t> get_new_biggest_topic_position(drogon::orm::DbClientPtr users_db, uint64_t user_id, const std::string& src_lang, const std::string& dst_lang) {
    auto result_position = co_await users_db->execSqlCoro("SELECT COALESCE(MAX(position), 0) AS biggest_position "
        "FROM (SELECT position FROM user_topics WHERE user_id = $1 AND src_lang = $2 AND dst_lang = $3 AND is_enabled = TRUE "
        "UNION ALL "
        "SELECT position FROM user_global_topics "
        "WHERE user_id = $1 AND src_lang = $2 AND dst_lang = $3 AND is_enabled = TRUE) AS combined_positions", user_id, src_lang, dst_lang
    );

    co_return result_position[0]["biggest_position"].as<int32_t>() + 1;
}
}
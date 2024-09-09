#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>

namespace core::sql {

static drogon::Task<> update_user_sequence_last_update_id(const drogon::orm::DbClientPtr& users_db, uint64_t user_id) {
    auto result = co_await users_db->execSqlCoro("update user_sequence set last_update_id = last_update_id + 1 where user_id = $1", user_id);
    if (!result.affectedRows()) {
        throw std::runtime_error("user_sequence_not_updated");
    }
    co_return;
}

static drogon::Task<std::string> get_user_topic_src_lang(const drogon::orm::DbClientPtr& users_db, uint64_t user_id, uint64_t topic_id) {
    auto result = co_await users_db->execSqlCoro("select src_lang from user_topics where user_id = $1 and topic_id = $2", user_id, topic_id);
    if (!result.size()) {
        throw std::runtime_error("topic_not_exists");
    }
    co_return result[0]["src_lang"].as<std::string>();
}

static drogon::Task<uint32_t> get_topic_new_source_id(const drogon::orm::DbClientPtr& users_db, uint64_t user_id, uint64_t topic_id) {
    auto result = co_await users_db->execSqlCoro("update user_topics "
        "set last_source_id = last_source_id + 1 where user_id = $1 and topic_id = $2 returning last_source_id", 
        user_id, topic_id
    );

    if (!result.affectedRows()) {
        throw std::runtime_error("topic_not_exists");
    }

    co_return result[0]["last_source_id"].as<uint32_t>();
}

static drogon::Task<> add_topic_source(const drogon::orm::DbClientPtr& users_db, uint64_t user_id, uint64_t topic_id, std::string title, 
    std::string type, std::string processed_words, std::string text = "", std::string fail_type = "") 
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
}

static drogon::Task<> update_topic_source(const drogon::orm::DbClientPtr& users_db, uint64_t user_id, uint64_t topic_id, uint32_t source_id, std::string title, 
    std::string type, std::string processed_words, std::string text = "", std::string fail_type = "") 
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

}
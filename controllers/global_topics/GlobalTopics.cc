#include "GlobalTopics.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"

namespace v1
{
Task<> GlobalTopics::addGlobalTopicWithUserTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto user_topic_id_str = req->getHeader("userTopicId");
    auto position_str = req->getHeader("position");

    if (user_topic_id_str.empty() || position_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto user_topic_id = local_utils::topics::decode_user_topic_id_str(user_topic_id_str);
        auto topic_words_sequence = co_await core::sql::get_topic_words_sequence_and_if_update(users_db, user_id, user_topic_id);

        auto topic_meta = co_await users_db->execSqlCoro("select src_lang, topic_title from user_topics where user_id = $1 and topic_id = $2",
            user_id, user_topic_id
        );

        if (topic_meta.empty()) {
            std::runtime_error("empty_topic_metadata");
        }

        auto src_lang = topic_meta[0]["src_lang"].as<std::string>();
        auto src_title_en = topic_meta[0]["topic_title"].as<std::string>();
        auto default_position = std::stoi(position_str);

        auto result = co_await users_db->execSqlCoro("insert into global_topics (src_lang, src_title_en, default_position, words) "
            "values ($1, $2, $3, $4)", src_lang, src_title_en, default_position, topic_words_sequence
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("global_topic_not_created");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> GlobalTopics::deleteGlobalTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto global_topic_id_str = req->getHeader("globalTopicId");

    if (global_topic_id_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto topic_id = local_utils::topics::decode_global_topic_id_str(global_topic_id_str);
        co_await users_db->execSqlCoro("delete from global_topics where topic_id = $1", topic_id);

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> GlobalTopics::updateGlobalTopicWithUserTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto global_topic_id_str = req->getHeader("globalTopicId");
    auto user_topic_id_str = req->getHeader("userTopicId");
    auto position_str = req->getHeader("position");

    if (user_topic_id_str.empty() || position_str.empty() || global_topic_id_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto user_topic_id = local_utils::topics::decode_user_topic_id_str(user_topic_id_str);
        auto global_topic_id = local_utils::topics::decode_global_topic_id_str(global_topic_id_str);
        auto topic_words_sequence = co_await core::sql::get_topic_words_sequence_and_if_update(users_db, user_id, user_topic_id);

        auto topic_meta = co_await users_db->execSqlCoro("select src_lang, topic_title from user_topics where user_id = $1 and topic_id = $2",
            user_id, user_topic_id
        );

        if (topic_meta.empty()) {
            std::runtime_error("empty_topic_metadata");
        }

        auto src_lang = topic_meta[0]["src_lang"].as<std::string>();
        auto src_title_en = topic_meta[0]["topic_title"].as<std::string>();
        auto default_position = std::stoi(position_str);

        auto result = co_await users_db->execSqlCoro("update global_topics set src_lang = $1, src_title_en= $2, "
            "default_position = $3, words = $4 where topic_id = $5", 
            src_lang, src_title_en, default_position, topic_words_sequence, global_topic_id
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("global_topic_not_updated");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> GlobalTopics::getGlobalTopics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    Json::Value result_body;

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto result = co_await users_db->execSqlCoro("select topic_id, src_lang, src_title_en, default_position from global_topics");
        result_body = createTopicsList(result);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Json::Value GlobalTopics::createTopicsList(const drogon::orm::Result& rows) {
    Json::Value result;
    auto& topics = result["global_topics"] = Json::arrayValue;

    for (auto& row : rows) {
        Json::Value new_topic;
        auto topic_id = row["topic_id"].as<uint64_t>();
        new_topic["topic_id"] = local_utils::topics::encode_topic_id(topic_id, local_utils::topics::TopicType::global_topic);
        new_topic["src_lang"] = row["src_lang"].as<std::string>();
        new_topic["src_title_en"] = row["src_title_en"].as<std::string>();
        new_topic["default_position"] = row["default_position"].as<int32_t>();
        topics.append(new_topic);
    }

    return result;
}
}
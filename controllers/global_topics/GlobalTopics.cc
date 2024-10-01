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

        // co_await users_db->exec

        // auto result = co_await users_db->execSqlCoro("insert into global_topics");

        // if (!result.affectedRows()) {
        //     throw std::runtime_error("topic_not_created");
        // }

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
        auto [topic_id, topic_type] = local_utils::topics::decode_topic_id_str(global_topic_id_str);

        if (topic_type != local_utils::topics::TopicType::global_topic) {
            throw std::runtime_error("not_valid_topic_type");
        }

        co_await users_db->execSqlCoro("delete from global_topics where topic_id = $1", topic_id);

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> GlobalTopics::updateGlobalTopicWithUserTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto user_topic_id_str = req->getHeader("userTopicId");
    auto global_topic_id_str = req->getHeader("globalTopicId");
    auto position_str = req->getHeader("position");

    if (user_topic_id_str.empty() || global_topic_id_str.empty() || position_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}
}
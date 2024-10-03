#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

using namespace drogon;

namespace v1
{
class GlobalTopics : public drogon::HttpController<GlobalTopics>
{
public:
    Task<> addGlobalTopicWithUserTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
    Task<> deleteGlobalTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
    Task<> updateGlobalTopicWithUserTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
    Task<> getGlobalTopics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

    METHOD_LIST_BEGIN
    METHOD_ADD(GlobalTopics::addGlobalTopicWithUserTopic, "/add_global_topic_with_user_topic", Post, "JwtFilter", "AdminFilter");
    METHOD_ADD(GlobalTopics::deleteGlobalTopic, "/delete_global_topic", Post, "JwtFilter", "AdminFilter");
    METHOD_ADD(GlobalTopics::updateGlobalTopicWithUserTopic, "/update_global_topic_with_user_topic", Post, "JwtFilter", "AdminFilter");
    METHOD_ADD(GlobalTopics::getGlobalTopics, "/get_global_topics", Get, "JwtFilter", "AdminFilter");
    METHOD_LIST_END

private:
    Json::Value createTopicsList(const drogon::orm::Result& rows);
};
}
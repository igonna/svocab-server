#pragma once

#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class UserTopics : public drogon::HttpController<UserTopics>
{
public:
	Task<> createTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getTopics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getTopicProgress(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> renameTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> updateTopicContent(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> deleteTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

    METHOD_LIST_BEGIN
    METHOD_ADD(UserTopics::createTopic, "/create_topic", Post, "JwtFilter");
	METHOD_ADD(UserTopics::getTopics, "/get_topic", Get, "JwtFilter");
	METHOD_ADD(UserTopics::getTopicProgress, "/get_topic_progress", Get, "JwtFilter");
	METHOD_ADD(UserTopics::renameTopic, "/rename_topic", Post, "JwtFilter");
	METHOD_ADD(UserTopics::updateTopicContent, "/update_topic_content", Post, "JwtFilter");
	METHOD_ADD(UserTopics::deleteTopic, "/delete_topic", Post, "JwtFilter");
    METHOD_LIST_END
private:
	Json::Value createTopicGenerationWindow(const drogon::orm::Result::Reference& row, const std::string& to_translate_lang_tag);
	Json::Value createTopicGenerationItem(uint32_t id, const std::string& title, const std::string& type, const std::string& fail_type = "", bool topic_title_updated = false);
	Json::Value createTopicJson(const drogon::orm::Row &row);
	Json::Value createTopicsList(const drogon::orm::Result& lang_pairs_rows, const drogon::orm::Result& topics_rows, const std::string& to_translate_lang);
};
}
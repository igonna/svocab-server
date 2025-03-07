#pragma once

#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class Topics : public drogon::HttpController<Topics>
{
public:
	Task<> createTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> appendContent(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getTopicsTitles(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getTopics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getTopicProgress(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> renameTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> updateTopicContent(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> deleteTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getSupportedDocumentTypes(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

    METHOD_LIST_BEGIN
    METHOD_ADD(Topics::createTopic, "/create_topic", Post, "JwtFilter");
	METHOD_ADD(Topics::appendContent, "/append_content", Post, "JwtFilter");
	METHOD_ADD(Topics::getTopicsTitles, "/get_current_topics_titles", Get, "JwtFilter");
	METHOD_ADD(Topics::getTopics, "/get_topics", Get, "JwtFilter");
	METHOD_ADD(Topics::getTopicProgress, "/get_topic_progress", Get, "JwtFilter");
	METHOD_ADD(Topics::renameTopic, "/rename_topic", Post, "JwtFilter");
	METHOD_ADD(Topics::updateTopicContent, "/update_topic_content", Post, "JwtFilter");
	METHOD_ADD(Topics::deleteTopic, "/delete_topic", Post, "JwtFilter");
	METHOD_ADD(Topics::getSupportedDocumentTypes, "/get_supported_document_types", Get, "JwtFilter");
    METHOD_LIST_END

private:
	Json::Value createTopicsList(const drogon::orm::Result& lang_pairs_rows, const drogon::orm::Result& topics_rows, const std::string& to_translate_lang);
	Json::Value createTopicsTitlesList(const drogon::orm::Result& topics_rows);
};
}
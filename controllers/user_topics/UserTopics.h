#pragma once

#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class UserTopics : public drogon::HttpController<UserTopics>
{
public:
	Task<> enableFirstUserTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> createTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> deleteTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getUserTopics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getTopicGenerationWindow(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> setTopicTitle(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> addTopicSourceText(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> addTopicSourceDocument(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> addTopicSourceWebsiteUrl(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> addTopicSourceYoutubeUrl(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> deleteTopicSource(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getTopicSourceTextContent(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> setTopicSourceText(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> setTopicOption(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> setTopicEnabled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> setTopicPosition(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getTopicWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getTopicMixWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

    METHOD_LIST_BEGIN
	METHOD_ADD(UserTopics::enableFirstUserTopic, "/enable_first_user_topic", Post, "JwtFilter");
    METHOD_ADD(UserTopics::createTopic, "/create_topic", Post, "JwtFilter");
	METHOD_ADD(UserTopics::deleteTopic, "/delete_topic", Post, "JwtFilter");
	METHOD_ADD(UserTopics::getUserTopics, "/get_user_topics", Get, "JwtFilter");
	METHOD_ADD(UserTopics::getTopicGenerationWindow, "/get_topic_generation_window", Get, "JwtFilter");
	METHOD_ADD(UserTopics::setTopicTitle, "/set_topic_title", Post, "JwtFilter");
	METHOD_ADD(UserTopics::addTopicSourceText, "/add_topic_source_text", Post, "JwtFilter");
	METHOD_ADD(UserTopics::addTopicSourceDocument, "/add_topic_source_document", Post, "JwtFilter");
	METHOD_ADD(UserTopics::addTopicSourceWebsiteUrl, "/add_topic_source_website_url", Post, "JwtFilter");
	METHOD_ADD(UserTopics::addTopicSourceYoutubeUrl, "/add_topic_source_youtube", Post, "JwtFilter");
	METHOD_ADD(UserTopics::deleteTopicSource, "/delete_topic_source", Post, "JwtFilter");
	METHOD_ADD(UserTopics::getTopicSourceTextContent, "/get_topic_source_text_content", Get, "JwtFilter");
	METHOD_ADD(UserTopics::setTopicSourceText, "/set_topic_source_text", Post, "JwtFilter");
	METHOD_ADD(UserTopics::setTopicOption, "/set_topic_option", Post, "JwtFilter");
	METHOD_ADD(UserTopics::setTopicEnabled, "/set_topic_enabled", Post, "JwtFilter");
	METHOD_ADD(UserTopics::setTopicPosition, "/set_topic_position", Post, "JwtFilter");
	METHOD_ADD(UserTopics::getTopicWords, "/get_topic_words", Get, "JwtFilter");
	METHOD_ADD(UserTopics::getTopicMixWords, "/get_topic_mix_words", Get, "JwtFilter");
    METHOD_LIST_END
private:
	Json::Value createTopicGenerationWindow(const drogon::orm::Result::Reference& row, const std::string& to_translate_lang_tag);
	Json::Value createTopicGenerationItem(uint32_t id, const std::string& title, const std::string& type, const std::string& fail_type = "");
	Json::Value createTopicsList(const drogon::orm::Result& rows, const std::string& to_translate_lang_tag);
};
}
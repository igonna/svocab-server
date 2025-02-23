#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class Progress : public drogon::HttpController<Progress>
{
public:
	Task<> getHiddenWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> removeHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(Progress::getHiddenWords, "/get_hidden_words", Get, "JwtFilter");
	METHOD_ADD(Progress::removeHiddenWord, "/remove_hidden_word", Post, "JwtFilter");
	METHOD_LIST_END
};
}
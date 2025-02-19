#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserProgress : public drogon::HttpController<UserProgress>
{
public:
	Task<> getHiddenWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> removeHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserProgress::getHiddenWords, "/get_hidden_words", Get, "JwtFilter");
	METHOD_ADD(UserProgress::removeHiddenWord, "/remove_hidden_word", Post, "JwtFilter");
	METHOD_LIST_END
};
}
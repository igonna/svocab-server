#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class UserBareToken : public drogon::HttpController<UserBareToken>
{
public:
	Task<> getBareToken(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserBareToken::getBareToken, "/get", Get);
	METHOD_LIST_END
};

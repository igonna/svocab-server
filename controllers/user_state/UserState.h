#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserState : public drogon::HttpController<UserState>
{
public:
	Task<> getCheckpoints(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserState::getCheckpoints, "/get_checkpoints", Get, "JwtFilter");
	METHOD_LIST_END
};
}
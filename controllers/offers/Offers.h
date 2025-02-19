#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserCheckpoints : public drogon::HttpController<UserCheckpoints>
{
public:
	Task<> setCheckpoint(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserCheckpoints::setCheckpoint, "/set_checkpoint", Post, "JwtFilter");
	METHOD_LIST_END
};
}
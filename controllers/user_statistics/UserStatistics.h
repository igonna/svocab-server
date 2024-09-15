#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserStatistics : public drogon::HttpController<UserStatistics>
{
public:
	Task<> getUserStatistics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserStatistics::getUserStatistics, "/get", Get, "JwtFilter");
	METHOD_LIST_END
};
}
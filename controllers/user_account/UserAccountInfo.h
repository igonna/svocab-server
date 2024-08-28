#pragma once

#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class UserAccountInfo : public drogon::HttpController<UserAccountInfo>
{
public:
	Task<> getAccountInfo(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserAccountInfo::getAccountInfo, "/get_info", Post, "JwtFilter");
	METHOD_LIST_END
};
}
#pragma once

#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class UserDevices : public drogon::HttpController<UserDevices>
{
public:
  	Task<> getBareToken(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> logOutOfAllDevices(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserDevices::getBareToken, "/get_bare_token", Get);
	METHOD_ADD(UserDevices::logOutOfAllDevices, "/log_out_of_all_devices", Post, "JwtFilter");
	METHOD_LIST_END
};
};
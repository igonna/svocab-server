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
	Task<> getDevices(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> revokeDevice(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserDevices::getBareToken, "/get_bare_token", Post);
	METHOD_ADD(UserDevices::getDevices, "/get_devices", Get, "JwtFilter");
	METHOD_ADD(UserDevices::revokeDevice, "/revoke_device", Post, "JwtFilter");
	METHOD_LIST_END
};
};
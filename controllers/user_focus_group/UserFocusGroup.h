#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserFocusGroup : public drogon::HttpController<UserFocusGroup>
{
public:
	Task<> getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> updateFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserFocusGroup::getFocusGroup, "/get_focus_group", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::updateFocusGroup, "/update_focus_group", Post, "JwtFilter");
	METHOD_LIST_END
};
}
#pragma once
#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class UserDeleteAccount : public drogon::HttpController<UserDeleteAccount>
{
public:
	Task<> deleteAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserDeleteAccount::deleteAccount, "/delete_account", Post, "JwtFilter");
	METHOD_LIST_END
};
}
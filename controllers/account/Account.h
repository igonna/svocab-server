#pragma once

#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class Account : public drogon::HttpController<Account>
{
public:
	Task<> registerAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getAccountInfo(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> deleteAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	
	METHOD_LIST_BEGIN
	METHOD_ADD(Account::registerAccount, "/register", Post);
	METHOD_ADD(Account::getAccountInfo, "/get_info", Get, "JwtFilter");
	METHOD_ADD(Account::deleteAccount, "/delete_account", Post, "JwtFilter");
	METHOD_LIST_END
};
}
#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserRegister : public drogon::HttpController<UserRegister>
{
public:
	Task<> registerAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	
	METHOD_LIST_BEGIN
	METHOD_ADD(UserRegister::registerAccount, "/register", Post);
	METHOD_LIST_END
};
}
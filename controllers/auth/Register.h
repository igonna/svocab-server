#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class Register : public drogon::HttpController<Register>
{
public:
	Task<> registerAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	
	METHOD_LIST_BEGIN
	METHOD_ADD(Register::registerAccount, "/register", Post);
	METHOD_LIST_END
};
}
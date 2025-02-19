#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserSignIn : public drogon::HttpController<UserSignIn>
{
public:
	Task<> getCodeForEmailSignIn(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> signInEmail(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> signInGoogle(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserSignIn::getCodeForEmailSignIn, "/get_code_to_email", Get);
	METHOD_ADD(UserSignIn::signInEmail, "/sign_in_email", Post);
	METHOD_ADD(UserSignIn::signInGoogle, "/sign_in_google", Post);
	METHOD_LIST_END
};
}
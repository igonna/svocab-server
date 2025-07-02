#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class SignIn : public drogon::HttpController<SignIn>
{
public:
	Task<> getCodeForEmailSignIn(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> signInEmail(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> signInGoogle(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> signInApple(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(SignIn::getCodeForEmailSignIn, "/get_code_to_email", Get);
	METHOD_ADD(SignIn::signInEmail, "/sign_in_email", Post);
	METHOD_ADD(SignIn::signInGoogle, "/sign_in_google", Post);
	METHOD_ADD(SignIn::signInApple, "/sign_in_apple", Post);
	METHOD_LIST_END
};
}
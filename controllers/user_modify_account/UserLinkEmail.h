#pragma once

#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class UserLinkEmail : public drogon::HttpController<UserLinkEmail>
{
public:
	Task<> getCodeForEmailLink(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> linkEmailToAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> linkGoogleToAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserLinkEmail::getCodeForEmailLink, "/get_code_to_email", Post, "JwtFilter");
	METHOD_ADD(UserLinkEmail::linkEmailToAccount, "/link_email", Post);
	METHOD_ADD(UserLinkEmail::linkGoogleToAccount, "/link_google", Post);
	// list path definitions here;
	// PATH_ADD("/path", "filter1", "filter2", HttpMethod1, HttpMethod2...);
	METHOD_LIST_END
};
};
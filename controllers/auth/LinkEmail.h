#pragma once

#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class LinkEmail : public drogon::HttpController<LinkEmail>
{
public:
	Task<> getCodeForEmailLink(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> linkEmailToAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> linkGoogleToAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(LinkEmail::getCodeForEmailLink, "/get_code_to_email", Get, "JwtFilter");
	METHOD_ADD(LinkEmail::linkEmailToAccount, "/link_email", Post, "JwtFilter");
	METHOD_ADD(LinkEmail::linkGoogleToAccount, "/link_google", Post, "JwtFilter");
	METHOD_LIST_END
};
};
#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class Subscription : public drogon::HttpController<Subscription>
{
public:
    Task<> update(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
    METHOD_ADD(Subscription::update, "/update", Post, "JwtFilter");
	METHOD_LIST_END
};
}
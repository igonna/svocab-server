#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class Offers : public drogon::HttpController<Offers>
{
public:
	Task<> setCheckpoint(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(Offers::setCheckpoint, "/set_checkpoint", Post, "JwtFilter");
	METHOD_LIST_END
};
}
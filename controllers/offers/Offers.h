#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class Offers : public drogon::HttpController<Offers>
{
public:
	Task<> setOfferPassed(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(Offers::setOfferPassed, "/set_offer_passed", Post, "JwtFilter");
	METHOD_LIST_END
};
}
#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class SupportedLanguages : public drogon::HttpController<SupportedLanguages>
{
public:
	Task<> getSupportedLanguages(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	METHOD_LIST_BEGIN
	METHOD_ADD(SupportedLanguages::getSupportedLanguages, "/get", Get);
	METHOD_LIST_END
};
}
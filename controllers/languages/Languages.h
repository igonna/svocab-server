#pragma once

#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class Languages : public drogon::HttpController<Languages>
{
public:
    Task<> getSupportedLanguages(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> setLanguagePairEnabled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

    METHOD_LIST_BEGIN
    METHOD_ADD(Languages::getSupportedLanguages, "/get_supported_languages", Get);
    METHOD_ADD(Languages::setLanguagePairEnabled, "/set_language_pair_enabled", Post, "JwtFilter");
    METHOD_LIST_END
};
}
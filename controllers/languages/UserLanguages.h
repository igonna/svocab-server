#pragma once

#include <drogon/HttpController.h>
#include "filters/JwtFilter.h"

using namespace drogon;

namespace v1
{
class UserLanguages : public drogon::HttpController<UserLanguages>
{
public:
	Task<> setLanguagePairEnabled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

    METHOD_LIST_BEGIN
    METHOD_ADD(UserLanguages::setLanguagePairEnabled, "/set_language_pair_enabled", Post, "JwtFilter");
    METHOD_LIST_END

private:
    Json::Value createLangPairWordsJson(bool is_this, std::string&& words, float mix_value);
};
}
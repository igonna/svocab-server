#include "SupportedLanguages.h"
#include "utils/constants.h"
#include "utils/utils.h"

namespace v1
{
Task<> SupportedLanguages::getSupportedLanguages(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto to_translate_lang = req->getHeader("toTranslateLang");

    if (to_translate_lang.size() != local_constants::languages::lang_tag_size) {
        local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    auto supported_languages_key = local_utils::redis::create_supported_languages_key(to_translate_lang);
    auto rc = app().getRedisClient();

    try {
        auto result = co_await rc->execCommandCoro("get %s", supported_languages_key.c_str());

        if (!result.isNil()) {
            co_return local_utils::Response::OK(callback, result.asString());
        }
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    std::string supported_languages = local_utils::supported_languages::get_supported_languages_as_string(to_translate_lang);

    try {
        co_await rc->execCommandCoro("set %s %s %s %d", 
            supported_languages_key.c_str(), 
            supported_languages.c_str(),
            "EX",
            local_constants::languages_str_expire_sconds
        );

    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, supported_languages);
}
}
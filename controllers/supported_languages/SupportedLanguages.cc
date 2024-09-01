#include "SupportedLanguages.h"
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include "utils/constants.h"
#include "utils/utils.h"

namespace v1
{
std::string get_language_title_localized(const std::string& lang_tag, const std::string& to_translate_lang_tag) {
    icu::Locale locale(lang_tag.c_str());
    icu::Locale display_locale(to_translate_lang_tag.c_str());
    
    icu::UnicodeString locale_name;
    locale.getDisplayLanguage(display_locale, locale_name);
    std::string locale_name_utf8;
    locale_name.toUTF8String(locale_name_utf8);

    return locale_name_utf8;
}

std::string get_supported_langauges_as_string(const std::unordered_set<std::string>& supported_locales, const std::string& to_translate_lang_tag) {
    std::string result;
    for (auto i : local_constants::languages::chat_gpt_supported_languages_sorted_by_learning_popularity) {
        if (supported_locales.contains(i)) {
            result += std::string(i) + ";" + get_language_title_localized(i, to_translate_lang_tag) + "\n";
        }
    }
    return result;
}

Task<> SupportedLanguages::getSupportedLanguages(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto to_translate_lang_tag = req->getHeader("toTranslateLangTag");

    if (to_translate_lang_tag.size() != local_constants::languages::lang_tag_size) {
        local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    static std::unordered_set<std::string> supported_locales = {};

    if (!supported_locales.size()) {
        int32_t count;
        const icu::Locale* locales = icu::Locale::getAvailableLocales(count);

        for (int32_t i = 0; i < count; ++i) {
            supported_locales.insert(locales[i].getName());
        }
    }


    auto supported_languages_key = local_utils::redis::create_supported_languages_key(to_translate_lang_tag);
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

    std::string supported_languages = get_supported_langauges_as_string(supported_locales, to_translate_lang_tag);

    try {
        co_await rc->execCommandCoro("set %s %s", 
            supported_languages_key.c_str(), 
            supported_languages.c_str()
        );
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, supported_languages);
}
}
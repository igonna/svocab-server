#include "utils.h"
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unordered_set>
#include <drogon/utils/coroutine.h>
#include <drogon/drogon.h>

namespace local_utils {
namespace random {
    std::unordered_set<uint32_t> get_random_nums_in_range(uint32_t min_num, uint32_t max_num, uint32_t count) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distr(min_num, max_num);

        std::unordered_set<uint32_t> result;

        if (count < 0 || min_num == max_num) return result;

        while (result.size() != count) {
            result.insert(distr(gen));
        }

        return result;
    }
}

namespace supported_languages {
    std::string get_language_title_localized(const std::string& lang_tag, const std::string& to_translate_lang_tag) {
        icu::Locale locale(lang_tag.c_str());
        icu::Locale display_locale(to_translate_lang_tag.c_str());
        
        icu::UnicodeString locale_name;
        locale.getDisplayLanguage(display_locale, locale_name);

        if (!locale_name.isEmpty()) {
            UChar upper_char = u_toupper(locale_name.charAt(0));
            locale_name.setCharAt(0, upper_char);
        }

        std::string locale_name_utf8;
        locale_name.toUTF8String(locale_name_utf8);

        return locale_name_utf8;
    }

    std::string get_supported_languages_as_string(const std::string& to_translate_lang_tag) {

        static std::unordered_set<std::string> supported_locales = {};

        if (!supported_locales.size()) {
            int32_t count;
            const icu::Locale* locales = icu::Locale::getAvailableLocales(count);

            for (int32_t i = 0; i < count; ++i) {
                supported_locales.insert(locales[i].getName());
            }
        }

        std::string result;
        for (auto i : local_constants::languages::chat_gpt_supported_languages_sorted_by_learning_popularity) {
            if (supported_locales.contains(i)) {
                result += std::string(i) + ';' + local_utils::supported_languages::get_language_title_localized(i, to_translate_lang_tag);

                try {
                    std::string country_code = local_constants::languages::language_to_country.at(i);

                    std::transform(country_code.begin(), country_code.end(), country_code.begin(), 
                        [](unsigned char c) { return std::tolower(c); }
                    );

                    result += std::string(";") + country_code;
                } catch (...) {}

                result += '\n';
            }
        }
        return result;
    }
}

namespace strings {
    std::string get_title_from_str(const std::string& str, uint32_t length) {
        std::string title;

        if (str.size() < length) {
            length = str.size();
        }

        for (uint32_t i = 0; i < length; ++i) {
            if (str[i] == '\n') {
                title += ' ';
            } else {
                title += str[i];
            }
        }

        return title;
    }
}
}
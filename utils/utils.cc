#include "utils.h"
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unordered_set>
#include <drogon/utils/coroutine.h>
#include <drogon/drogon.h>
#include <core/text_processing/text_processor.h>

namespace local_utils {
namespace random {
    uint32_t get_random_number(uint32_t min, uint32_t max) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(min, max);

        return dis(gen);
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

        if (supported_locales.empty()) {
            int32_t count;
            const icu::Locale* locales = icu::Locale::getAvailableLocales(count);

            for (int32_t i = 0; i < count; ++i) {
                supported_locales.insert(locales[i].getName());
            }
        }

        std::string result;
        for (const auto& lang_tag : local_constants::languages::ai_supported_languages_sorted_by_learning_popularity) {
            if (!supported_locales.contains(lang_tag)) continue;

            auto country_code_it = local_constants::languages::language_to_country.find(lang_tag);
            if (country_code_it == local_constants::languages::language_to_country.end()) continue;
            auto country_code = country_code_it->second;
            for(auto& c : country_code) c = tolower(c);

            result += lang_tag + ';' + get_language_title_localized(lang_tag, to_translate_lang_tag) + ';' + country_code + '\n';
        }
        return result;
    }

    std::string get_country_code_or_empty(const std::string& src_lang) {
        return local_constants::languages::language_to_country.contains(src_lang) 
                ? local_constants::languages::language_to_country.at(src_lang) 
                : "";
    }

    bool is_language_supported(const std::string& language) {
        return local_constants::languages::ai_supported_languages.contains(language);
    }
}

namespace strings {
    std::string get_title_from_str(const std::string& str, uint32_t length) {
        auto unicode_str = core::text_processing::string_to_unicode(str);
        if (unicode_str.length() < length) length = unicode_str.length();

        UChar32 prev_char = ' ';
        const UChar32 whitespace = ' ';

        icu::UnicodeString title;

        for (int32_t i = 0; i < length; ++i) {
            UChar32 c = unicode_str.char32At(i);

            if (u_isalpha(c)) {
                title += c;
            } else if (prev_char != whitespace) {
                title += whitespace;
            }

            prev_char = c;
        }

        if (title.length()) {
            title.setCharAt(0, u_toupper(title.char32At(0)));
        }

        return core::text_processing::unicode_to_str(title);
    }

    void replace_all(std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

    std::string get_capitalized_string(const std::string& str) {
        auto unicode_str = icu::UnicodeString::fromUTF8(icu::StringPiece(str.data()));

        if (!unicode_str.isEmpty()) {
            UChar upper_char = u_toupper(unicode_str.charAt(0));
            unicode_str.setCharAt(0, upper_char);
        }

        std::string utf8_str;
        unicode_str.toUTF8String(utf8_str);

        return utf8_str;
    }
}

namespace file {
    std::string get_string_from_file(std::string path) {
        std::ifstream ifile(path);
        if (!ifile)
            return std::string();
    
        std::ostringstream ss;
        ss << ifile.rdbuf();
        return ss.str();
    }
}
}
#include "text_processor.h"

#include <unicode/uchar.h>
#include <unicode/brkiter.h>
#include <memory>
#include <unordered_map>
#include <map>

#include <iostream>
#include <unicode/uscript.h>

#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <unicode/uloc.h>
#include <unordered_map>

namespace core::text_processing
{

enum class LID : int32_t {unknown = 0, en = 1033};

inline bool is_alpha_from_lang(UChar32 ch, LID lang_id) {
    if (lang_id == LID::en) {
        return (ch >= 0x41 && ch <= 0x5A) || (ch >= 0x61 && ch <= 0x7A);
    }
    return u_isalpha(ch);
}

LID get_lang_id_from_lang(const char* lang) {
    static const std::unordered_map<std::string, LID> ids = {{"en", LID::en}};

    try {
        return ids.at(lang);
    } catch (...) {
        return LID::unknown;
    }
}

uint32_t get_desired_script_code(const char* language) {
    UErrorCode status = U_ZERO_ERROR;
    UScriptCode script_buff[10] = {USCRIPT_INVALID_CODE};
    int script_count = uscript_getCode(language, script_buff, ULOC_KEYWORDS_CAPACITY, &status);

    uint32_t desired_script_code = 0;

    if (!U_FAILURE(status)) {
        if (script_count > 0) {
            desired_script_code = script_buff[0];
        }
    }
    return desired_script_code;
}

inline bool is_char_script_desired(UChar c, uint32_t desired_script_code) {
    UErrorCode status = U_ZERO_ERROR;
    UScriptCode script_code = uscript_getScript(c, &status);

    if (U_FAILURE(status)) {
        return true;
    }

    return script_code == desired_script_code;
}

std::string text_to_processed_source(icu::UnicodeString text, const char* language) {
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> words_data;
    
    uint32_t total_unique_words = 0;
    static const uint32_t unique_words_limit = 1'000'000;

    UErrorCode status = U_ZERO_ERROR;
    auto locale = icu::Locale(language);
    std::unique_ptr<icu::BreakIterator> bi(icu::BreakIterator::createWordInstance(locale, status));
    if (U_FAILURE(status)) {
        throw std::runtime_error("failed_to_process");
    }

    bi->setText(text);

    uint32_t desired_script_code = get_desired_script_code(language);

    auto lang_id = get_lang_id_from_lang(language);

    int32_t start = bi->first();
    for (int32_t end = bi->next(); end != icu::BreakIterator::DONE; start = end, end = bi->next()) {
        if (!u_isalpha(text[start])) continue;

        icu::UnicodeString word;
        text.extractBetween(start, end, word);
        word = word.toLower(locale);

        bool is_valid_word = true;
        for (int32_t i = 0; i < word.length(); ++i) {
            UChar32 c = word.char32At(i);

            if (!(is_alpha_from_lang(c, lang_id) && is_char_script_desired(c, desired_script_code)) || u_isdigit(c)) {
                is_valid_word = false;
                break;
            }
        }

        if (!is_valid_word) {
            continue;
        }

        std::string utf8_word;
        word.toUTF8String(utf8_word);

        if (words_data.contains(utf8_word)) {
            words_data[std::move(utf8_word)].first += 1;
        } else {
            words_data[std::move(utf8_word)] = {1, total_unique_words};
            ++total_unique_words;
            if (total_unique_words > unique_words_limit) break;
        }
    }

    // total_count, word, first_appearance
    std::multimap<uint32_t, std::pair<std::string, uint32_t>, std::greater<uint32_t>> words_data_by_popularity;

    for (auto& i : words_data) {
        words_data_by_popularity.insert({i.second.first, {i.first, i.second.second}});
    }

    words_data.clear();

    std::string result;
    for (auto& i : words_data_by_popularity) {
        // word, total_count, first_appearance
        result += i.second.first + ';' + std::to_string(i.first) + ';' + std::to_string(i.second.second) + '\n';
    }

    return result;
}

// word, total_count, first_appearance
std::vector<ProcessedWord> processed_words_str_to_binary(const std::string& processed_words_str) {
    std::vector<ProcessedWord> result;

    auto len = processed_words_str.size();

    uint32_t part_num = 0;
    std::string tmp_str;

    std::string current_word;
    uint32_t word_total_count;

    for (uint32_t i = 0; i < len; ++i) {
        auto current_char = processed_words_str[i];

        if (current_char == ';') {
            if (part_num == 1) {
                word_total_count = std::stoi(tmp_str);
            }

            ++part_num;
            tmp_str.clear();
            continue;
        }

        if (part_num == 0) {
            current_word += current_char;
        } else {
            tmp_str += current_char;
        }

        if (current_char == '\n' || i == len) {
            result.push_back(ProcessedWord(current_word, word_total_count, std::stoi(tmp_str)));
            current_word.clear();
            tmp_str.clear();
            part_num = 0;
        }
    }

    return result;
}
}
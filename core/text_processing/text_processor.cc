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
#include <unordered_set>
#include <unicode/utypes.h>

#include <algorithm>
#include <random>
#include <chrono>

namespace core::text_processing
{

UChar32 get_apostrophe_for_lang(const std::string& lang) {
    static const std::unordered_map<std::string, UChar32> apostrophes_per_lang = {
        // Languages with U+2019 (Typographic Apostrophe)
        {"en", 0x2019},  // English
        {"fr", 0x2019},  // French
        {"de", 0x2019},  // German
        {"it", 0x2019},  // Italian
        {"pt", 0x2019},  // Portuguese
        {"nl", 0x2019},  // Dutch
        {"sv", 0x2019},  // Swedish
        {"no", 0x2019},  // Norwegian
        {"da", 0x2019},  // Danish
        {"ro", 0x2019},  // Romanian
        {"fi", 0x2019},  // Finnish
        {"ca", 0x2019},  // Catalan
        {"hu", 0x2019},  // Hungarian
        {"pl", 0x2019},  // Polish
        {"cs", 0x2019},  // Czech
        {"sk", 0x2019},  // Slovak
        {"sl", 0x2019},  // Slovenian
        {"lt", 0x2019},  // Lithuanian
        {"lv", 0x2019},  // Latvian
        {"et", 0x2019},  // Estonian
        {"sq", 0x2019},  // Albanian
        {"bs", 0x2019},  // Bosnian
        {"hr", 0x2019},  // Croatian
        {"sr", 0x2019},  // Serbian
        {"bg", 0x2019},  // Bulgarian

        // Special Characters for Specific Languages
        {"haw", 0x02BB}, // Hawaiian (ʻokina)
        {"hy", 0x055A},  // Armenian apostrophe (U+055A)
        {"ka", 0x2019},  // Georgian (generally uses U+2019 in modern typography)
        {"kk", 0x02BC},  // Kazakh (glottal stop apostrophe)
        {"mn", 0x0027},  // Mongolian uses straight apostrophe
        {"my", 0x0027},  // Burmese (no strict apostrophe, U+0027 can be used informally)
        {"tl", 0x2019},  // Filipino/Tagalog (Typographic apostrophe)
        {"am", 0x0027},  // Amharic (informally uses straight apostrophe)

        // Languages with U+0027 (Straight Apostrophe)
        {"es", 0x0027},  // Spanish (uses straight apostrophe)
        {"zh", 0x0027},  // Chinese (rarely uses apostrophes, U+0027 when needed)
        {"ja", 0x0027},  // Japanese (rarely uses apostrophes, U+0027 when needed)
        {"ru", 0x0027},  // Russian (rarely uses apostrophes, U+0027 when needed)
        {"ko", 0x0027},  // Korean (U+0027, though rarely used)
        {"ar", 0x0027},  // Arabic (informally U+0027, though apostrophes aren't common)
        {"tr", 0x0027},  // Turkish (straight apostrophe)
        {"hi", 0x0027},  // Hindi (informal use of U+0027)
        {"vi", 0x0027},  // Vietnamese (straight apostrophe used in informal writing)
        {"id", 0x0027},  // Indonesian (U+0027 is the default)
        {"th", 0x0027},  // Thai (informal use of U+0027)
        {"uk", 0x0027},  // Ukrainian (rarely used, U+0027 in informal contexts)
        {"fa", 0x0027},  // Persian (rarely uses apostrophes, U+0027 when needed)
        {"sw", 0x0027},  // Swahili (informal use of U+0027)
        {"bn", 0x0027},  // Bengali (straight apostrophe used informally)
        {"is", 0x0027},  // Icelandic (U+0027 is used for contractions)
        {"ms", 0x0027},  // Malay (informal use of U+0027)
        {"mk", 0x0027},  // Macedonian (rarely uses apostrophes, U+0027 when needed)
        {"ta", 0x0027},  // Tamil (informal use of U+0027)
        {"te", 0x0027},  // Telugu (informal use of U+0027)
        {"ur", 0x0027},  // Urdu (informally uses straight apostrophe)
        {"mr", 0x0027},  // Marathi (U+0027 is often used in informal contexts)
        {"gu", 0x0027},  // Gujarati (informally uses U+0027)
        {"ml", 0x0027},  // Malayalam (informal use of U+0027)
        {"kn", 0x0027},  // Kannada (informal use of U+0027)
        {"so", 0x0027},  // Somali (U+0027 is commonly used for contractions)
        {"pa", 0x0027},  // Punjabi (informal use of U+0027)
    };

    try {
        return apostrophes_per_lang.at(lang);
    } catch (...) {
        return 0x0027;
    }
}

inline bool is_apostrophe(UChar32 c) {
    static const std::unordered_set<UChar32> apostrophe_symbols = {
        0x0027,  // Apostrophe (Straight single quote) - ' (U+0027)
        0x2019,  // Right single quotation mark (Typographic apostrophe) - ’ (U+2019)
        0x02BC,  // Modifier letter apostrophe (Glottal stop) - ʼ (U+02BC)
        0x275C,  // Heavy single comma quotation mark ornament - ❜ (U+275C)
        0xFF07,  // Fullwidth apostrophe - ＇ (U+FF07)
        0x2018,  // Left single quotation mark (used as an opening apostrophe) - ‘ (U+2018)
        0x02BB,  // Modifier letter turned comma (ʻokina, used in Hawaiian) - ʻ (U+02BB)
        0xA78C   // Latin small letter saltillo (used in some native Latin American languages) - ꞌ (U+A78C)
    };

    return apostrophe_symbols.contains(c);
}

std::unordered_set<uint32_t> get_desired_script_codes(const char* language) {
    UErrorCode status = U_ZERO_ERROR;
    UScriptCode script_buff[10] = {USCRIPT_INVALID_CODE};
    int script_count = uscript_getCode(language, script_buff, ULOC_KEYWORDS_CAPACITY, &status);

    std::unordered_set<uint32_t> desired_script_codes;

    if (U_FAILURE(status) || script_count < 1) {
        return desired_script_codes;
    }

    for (uint32_t i = 0; i < script_count; ++i) {
        desired_script_codes.insert(script_buff[i]);
    }

    desired_script_codes.insert(static_cast<uint32_t>(USCRIPT_INHERITED));

    return desired_script_codes;
}

inline std::string unicode_to_str(const icu::UnicodeString& unicode_str) {
    std::string utf8Str;
    unicode_str.toUTF8String(utf8Str);
    return utf8Str;
}

inline bool is_char_script_desired(UChar c, const std::unordered_set<uint32_t>& desired_script_codes) {
    UErrorCode status = U_ZERO_ERROR;
    UScriptCode script_code = uscript_getScript(c, &status);

    if (U_FAILURE(status)) {
        return true;
    }

    return desired_script_codes.contains(script_code);
}

enum class LID : int32_t {unknown = 0, en = 1, ja = 2, ca = 3};

LID get_lang_id_from_lang(const char* lang) {
    static const std::unordered_map<std::string, LID> ids = {
        {"en", LID::en},
        {"ja", LID::ja},
        {"ca", LID::ca}
    };

    try {
        return ids.at(lang);
    } catch (...) {
        return LID::unknown;
    }
}

inline bool is_alpha_from_lang(UChar32 ch, LID lang_id) {
    if (lang_id == LID::en) {
        return (ch >= 0x41 && ch <= 0x5A) || (ch >= 0x61 && ch <= 0x7A);
    }
    return u_isalpha(ch);
}

inline bool is_non_spacing_mark_required(const std::string& language) {
    static const std::unordered_set<std::string> non_spacing_mark_langs = {"ar", "fa", "mr", "hi", "ta", "te", "kn", "ml", "bn", "gu", "pa"};
    return non_spacing_mark_langs.contains(language);
}

namespace special_chars {
    static const std::unordered_set<UChar32> japanese = {0x30FC};
    static const std::unordered_set<UChar32> catalan = {0xB7};
}

bool is_word_char_valid(UChar32 c, LID lang_id, const std::unordered_set<uint32_t>& desired_script_codes, bool non_spacing_mark_required) {
    auto char_type = u_charType(c);

    if (is_alpha_from_lang(c, lang_id)) {
        if (is_char_script_desired(c, desired_script_codes)) return true;

        // if (char_type == U_LOWERCASE_LETTER || char_type == U_UPPERCASE_LETTER || char_type == U_OTHER_LETTER) {
        //     return true;
        // }
    } else {
        if (non_spacing_mark_required && char_type == U_NON_SPACING_MARK) return true;
    }

    // language specific
    if (lang_id == LID::ja) {
        if (special_chars::japanese.contains(c)) return true;
    } else if (lang_id == LID::ca) {
        if (special_chars::catalan.contains(c)) return true;
    }

    return false;
}

std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> get_words_data_from_text(icu::UnicodeString&& text, const char* language) {
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

    auto desired_script_codes = get_desired_script_codes(language);

    auto lang_id = get_lang_id_from_lang(language);

    auto non_spacing_mark_required = is_non_spacing_mark_required(language);

    int32_t start = bi->first();
    for (int32_t end = bi->next(); end != icu::BreakIterator::DONE; start = end, end = bi->next()) {
        if (!u_isalpha(text[start])) continue;

        icu::UnicodeString word;
        text.extractBetween(start, end, word);
        word = word.toLower(locale);

        bool is_valid_word = true;
        for (int32_t i = 0; i < word.length(); ++i) {
            UChar32 c = word.char32At(i);

            if (u_isdigit(c)) {
                is_valid_word = false;
                break;
            }

            if (is_apostrophe(c)) {
                word.setCharAt(i, get_apostrophe_for_lang(language));
            } else {
                if (!is_word_char_valid(c, lang_id, desired_script_codes, non_spacing_mark_required)) {
                    is_valid_word = false;
                    break;
                }
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

    return words_data;
}

std::string text_to_processed_source(icu::UnicodeString&& text, const char* language) {
    auto words_data = get_words_data_from_text(std::move(text), language);

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
std::vector<ProcessedWord> processed_words_str_to_vector(const std::string& processed_words_str) {
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

std::string get_words_sequence_without_duplicates(const std::string& words_sequence, std::unordered_set<std::string>& words_to_exclude) {
    std::string result;

    std::string word;
    for (auto c : words_sequence) {
        if (c == ';') {
            if (!words_to_exclude.contains(word)) {
                result += word + ';';
                words_to_exclude.insert(word);
            }
            word.clear();
        } else {
            word += c;
        }
    }

    if (word.size() && !words_to_exclude.contains(word)) {
        result += word;
        words_to_exclude.insert(word);
    }

    if (result.size() && result.back() == ';') {
        result.pop_back();
    }

    return result;
}

std::vector<std::string> extract_words_and_punctuation_from_str(const std::string& str, const char* language) {
    auto unicode_str = core::text_processing::string_to_unicode(str);
    
    UErrorCode status = U_ZERO_ERROR;
    auto locale = icu::Locale(language);
    std::unique_ptr<icu::BreakIterator> bi(icu::BreakIterator::createWordInstance(locale, status));
    if (U_FAILURE(status)) {
        throw std::runtime_error("failed_to_process");
    }

    bi->setText(unicode_str);

    std::vector<std::string> result;

    int32_t start = bi->first();
    for (int32_t end = bi->next(); end != icu::BreakIterator::DONE; start = end, end = bi->next()) {

        icu::UnicodeString word_or_punctuation;
        unicode_str.extractBetween(start, end, word_or_punctuation);

        std::string utf8_word_or_punctuation;
        word_or_punctuation.toUTF8String(utf8_word_or_punctuation);
        result.push_back(std::move(utf8_word_or_punctuation));
    }

    return result;
}

std::string unicode_str_to_lower(const std::string& str, const char* language) {
    auto locale = icu::Locale(language);

    auto unicode_str = core::text_processing::string_to_unicode(str);
    unicode_str = unicode_str.toLower(locale);

    std::string utf8_str;
    unicode_str.toUTF8String(utf8_str);

    return utf8_str;
}

std::string shuffle_string(const std::string& str, const char* language) {
    auto locale = icu::Locale(language);

    auto unicode_str = core::text_processing::string_to_unicode(str);
    unicode_str = unicode_str.toLower(locale);

    std::vector<UChar> chars_vec;
    chars_vec.assign(unicode_str.getBuffer(), unicode_str.getBuffer() + unicode_str.length());

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(chars_vec.begin(), chars_vec.end(), rd);

    icu::UnicodeString shuffled_str(chars_vec.data(), chars_vec.size());

    std::string utf8_str;
    shuffled_str.toUTF8String(utf8_str);

    return utf8_str;
}

std::string shuffle_string_in_the_middle(const std::string& str, const char* language) {
    auto locale = icu::Locale(language);

    auto unicode_str = core::text_processing::string_to_unicode(str);
    unicode_str = unicode_str.toLower(locale);

    std::vector<UChar> chars_vec;
    chars_vec.assign(unicode_str.getBuffer(), unicode_str.getBuffer() + unicode_str.length());

    size_t length = chars_vec.size();
    if (length <= 2) {
        return str;
    }

    size_t start = length / 3;
    size_t end = length * 2 / 3;

    std::random_device rd;
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    uint64_t seed = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    std::shuffle(chars_vec.begin() + start, chars_vec.begin() + end, std::default_random_engine(seed));

    icu::UnicodeString shuffled_str;
    shuffled_str.append(chars_vec.data(), start);
    shuffled_str.append(chars_vec.data() + start, end - start);
    shuffled_str.append(chars_vec.data() + end, length - end);

    std::string utf8_str;
    shuffled_str.toUTF8String(utf8_str);

    return utf8_str;
}


std::vector<std::string> extract_letters_as_strings(const std::string& str, const char* language) {
    auto locale = icu::Locale(language);

    auto unicode_str = core::text_processing::string_to_unicode(str);
    unicode_str = unicode_str.toLower(locale);

    std::vector<UChar> chars_vec;
    chars_vec.assign(unicode_str.getBuffer(), unicode_str.getBuffer() + unicode_str.length());

    std::vector<std::string> result;
    for (const auto c : chars_vec) {
        unicode_str = c;

        std::string utf8_str;
        unicode_str.toUTF8String(utf8_str);

        result.push_back(utf8_str);
    }

    return result;
}
}
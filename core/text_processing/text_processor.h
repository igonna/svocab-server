#pragma once

#include <unicode/unistr.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace core::text_processing
{
    struct ProcessedWord {
        ProcessedWord(
            std::string word,
            uint32_t total_count,
            uint32_t first_appearance
        ) : word(word), total_count(total_count), first_appearance(first_appearance) {}

        std::string word;
        uint32_t total_count;
        uint32_t first_appearance;
    };

    inline icu::UnicodeString string_to_unicode(const std::string& str) {
        return icu::UnicodeString::fromUTF8(icu::StringPiece(str.data()));
    }

    std::string unicode_to_str(const icu::UnicodeString& unicode_str);
    icu::UnicodeString& replace_apostrophe_for_lang(icu::UnicodeString& str, const std::string& lang);

    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> get_words_data_from_text(icu::UnicodeString&& text, const char* language);
    std::string text_to_processed_source(icu::UnicodeString&& text, const char* language);

    std::vector<ProcessedWord> processed_words_str_to_vector(const std::string& processed_words_str);

    std::string get_words_sequence_without_duplicates(const std::string& words_sequence, std::unordered_set<std::string>& words_to_exclude);

    std::vector<std::string> extract_words_and_punctuation_from_str(const std::string& str, const char* language);
    std::vector<std::string> extract_words_from_str(const std::string& str, const char* language);
    std::string unicode_str_to_lower(const std::string& str, const char* language);

    std::string shuffle_string(const std::string& str);
    icu::UnicodeString shuffle_unicode_string(const icu::UnicodeString& unicode_str);
    std::string shuffle_string_in_the_middle(const std::string& str, const char* language);
    std::vector<std::string> extract_letters_as_strings(const std::string& str, const char* language);
}
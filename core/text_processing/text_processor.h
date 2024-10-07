#pragma once

#include <unicode/unistr.h>
#include <vector>
#include <unordered_set>

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

    std::string text_to_processed_source(icu::UnicodeString text, const char* language);

    std::vector<ProcessedWord> processed_words_str_to_vector(const std::string& processed_words_str);

    std::string get_words_sequence_without_duplicates(const std::string& words_sequence, std::unordered_set<std::string>& words_to_exclude);
}
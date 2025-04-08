#pragma once

#include <unicode/unistr.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "plugins/FrequentWords/FrequentWords.h"

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
    std::vector<icu::UnicodeString> get_unicode_words_from_text_as_vector(const icu::UnicodeString& text, const char* language);
    std::vector<std::string> get_words_from_str_splited_by(const std::string& str, char separator);

    std::string string_vec_to_string(const std::vector<std::string>& vec);
    std::string unicode_vec_to_string(const std::vector<icu::UnicodeString>& vec);

    bool is_word_of_first_lang(const std::string& word, const char* lang_src, const char* lang_dst, FrequentWords *frequent_words_plugin);

    std::vector<std::string> create_prioritized_words_sequence(const std::string& str, FrequentWords *frequent_words_plugin, const char* language_src, const char* language_dst);
    std::string create_prioritized_words_sequence_as_str(const std::string& str, FrequentWords *frequent_words_plugin, const char* language_src, const char* language_dst);

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

    std::string get_str_untill_char(const std::string& str, char c);

    template <typename ContainerType, typename ItemType>
    std::vector<ItemType> mix_sequences(const std::vector<std::pair<uint32_t, ContainerType>>& sequences, int32_t max_items = -1) {
        using IteratorType = typename ContainerType::const_iterator;

        const auto sequences_to_mix = sequences.size();
        uint32_t sequences_mixed_count = 0;

        std::vector<ItemType> result;

        std::vector<std::tuple<uint32_t, IteratorType, IteratorType>> iterators;

        for (const auto& seq : sequences) {
            iterators.emplace_back(seq.first, seq.second.begin(), seq.second.end());
        }

        uint32_t total_added = 0;

        while (sequences_to_mix > sequences_mixed_count) {
            for (auto& seq : iterators) {
                auto& count = std::get<0>(seq);
                auto& cur_pos = std::get<1>(seq);
                auto& end_pos = std::get<2>(seq);

                if (cur_pos == end_pos) continue;

                for (uint32_t i = 0; i < count && cur_pos != end_pos; ++i, ++cur_pos) {
                    if (max_items > 0 && total_added++ == max_items) {
                        return result;
                    }

                    if constexpr (std::is_same_v<typename ContainerType::value_type,
                        std::pair<const typename ContainerType::key_type, typename ContainerType::mapped_type>>) {
                        result.push_back(cur_pos->second); 
                    } else {
                        result.push_back(*cur_pos); 
                    }
                }

                if (cur_pos == end_pos) ++sequences_mixed_count;
            }
        }

        return result;
    }
}
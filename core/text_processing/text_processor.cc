#include "text_processor.h"

#include <unicode/uchar.h>
#include <unicode/brkiter.h>
#include <memory>
#include <unordered_map>
#include <map>


// hello, hi, what, wow, hello, wow
// what, where


// total sum in all sources
// relative to the source

// sum: hello, wow, hi, what, where
// balanced: hello, what, wow, where



// count_word_as
    // relative_to_the_source = popularity_order
    // total_sum = number_of_words_per_source
// order_words_by
    // by_popularity 
// exclude_word_if

//word_by_relative_popularity;total_count;first_appearance

// relative popularity = total_count -> position order

namespace core::text_processing
{
    std::string text_to_processed_words(icu::UnicodeString text, const char* language) {
        std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> words_data;
        
        uint32_t total_unique_words = 0;

        UErrorCode status = U_ZERO_ERROR;
        std::unique_ptr<icu::BreakIterator> bi(icu::BreakIterator::createWordInstance(icu::Locale(language), status));
        if (U_FAILURE(status)) {
            throw std::runtime_error("failed_to_process");
        }

        bi->setText(text);

        int32_t start = bi->first();
        for (int32_t end = bi->next(); end != icu::BreakIterator::DONE; start = end, end = bi->next()) {
            if (!u_isalpha(text[start])) continue;

            icu::UnicodeString word;
            text.extractBetween(start, end, word);
            word = word.toLower();

            std::string utf8_word;
            word.toUTF8String(utf8_word);
            
            if (words_data.contains(utf8_word)) {
                words_data[std::move(utf8_word)].first += 1;
            } else {
                words_data[std::move(utf8_word)] = {1, total_unique_words};
                ++total_unique_words;
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
}
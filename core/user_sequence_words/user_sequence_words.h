#pragma once

#include <string>
#include <vector>

namespace core::user_sequence_words {

    struct SequenceUserWord {
        SequenceUserWord(SequenceUserWord&& other) : src_lang(std::move(other.src_lang)), dst_lang(std::move(other.dst_lang)),
            word(std::move(other.word)), confirmation_level(other.confirmation_level), is_passed(other.is_passed), is_excluded(other.is_excluded) {}

        SequenceUserWord(const std::string& src_lang, const std::string& dst_lang, std::string&& word, uint8_t confirmation_level, bool is_passed, bool is_excluded)
            : src_lang(src_lang), dst_lang(dst_lang), word(std::move(word)), confirmation_level(confirmation_level), is_passed(is_passed), is_excluded(is_excluded) {}

        SequenceUserWord(std::string&& src_lang, std::string&& dst_lang, std::string&& word, uint8_t confirmation_level, bool is_passed, bool is_excluded)
            : src_lang(std::move(src_lang)), dst_lang(std::move(dst_lang)), word(std::move(word)), confirmation_level(confirmation_level), is_passed(is_passed), is_excluded(is_excluded) {}

        std::string src_lang;
        std::string dst_lang;
        std::string word;
        uint8_t confirmation_level;
        bool is_passed;
        bool is_excluded;
    };

    std::string sequence_user_words_to_str(const std::vector<SequenceUserWord>& sequence_user_words);
    std::vector<SequenceUserWord> sequence_user_words_str_to_vector(const std::string& words_str);
}
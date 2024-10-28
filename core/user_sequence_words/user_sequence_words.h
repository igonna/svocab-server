#pragma once

#include <string>

namespace core::user_sequence_words {

    struct SequenceUserWord {
        SequenceUserWord(SequenceUserWord&& other) : src_lang(std::move(other.src_lang)), dst_lang(std::move(other.dst_lang)),
            word(std::move(other.word)), confirmation_level(other.confirmation_level), is_passed(other.is_passed), is_excluded(other.is_excluded) {}

        SequenceUserWord(const std::string& src_lang, const std::string dst_lang, std::string&& word, uint8_t confirmation_level, bool is_passed, bool is_excluded)
            : src_lang(src_lang), dst_lang(dst_lang), word(std::move(word)), confirmation_level(confirmation_level), is_passed(is_passed), is_excluded(is_excluded) {}

        std::string src_lang;
        std::string dst_lang;
        std::string word;
        uint8_t confirmation_level;
        bool is_passed;
        bool is_excluded;
    };

    static std::string sequence_user_words_to_str(const std::vector<SequenceUserWord>& sequence_user_words) {
        std::string result;

        for (auto& user_word : sequence_user_words) {
            result += user_word.src_lang + ';' + user_word.dst_lang + ';' + user_word.word + ';' + std::to_string(user_word.confirmation_level) + ';' + std::to_string(user_word.is_passed) + ';' + std::to_string(user_word.is_excluded) + ';';
        }

        return result;
    }
}
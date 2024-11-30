#include "user_sequence_words.h"
#include <iostream>

namespace core::user_sequence_words {

    std::string sequence_user_words_to_str(const std::vector<SequenceUserWord>& sequence_user_words) {
        std::string result;

        for (auto& user_word : sequence_user_words) {
            result += user_word.src_lang + ';' + user_word.dst_lang + ';' + user_word.word + ';' + std::to_string(user_word.confirmation_level) + ';' + std::to_string(user_word.is_passed) + ';';
        }

        return result;
    }

    std::vector<SequenceUserWord> sequence_user_words_str_to_vector(const std::string& words_str) {
        std::vector<SequenceUserWord> result;

        auto len = words_str.size();

        uint32_t part_num = 0;
        std::string tmp_str;
        
        std::string src_lang;
        std::string dst_lang;
        std::string word;
        uint32_t confirmation_level;
        bool is_passed;

        uint32_t duplicates_count = 0;

        for (uint32_t i = 0; i < len; ++i) {
            auto current_char = words_str[i];

            if (current_char == ';') {
                if (part_num == 0) {
                    src_lang = std::move(tmp_str);
                } else if (part_num == 1) {
                    dst_lang = std::move(tmp_str);
                } else if (part_num == 2) {
                    word = std::move(tmp_str);
                } else if (part_num == 3) {
                    confirmation_level = std::stoi(tmp_str);
                } else if (part_num == 4) {
                    is_passed = tmp_str[0] == '0' ? false : true;
                    result.push_back(SequenceUserWord(std::move(src_lang), std::move(dst_lang), std::move(word), confirmation_level, is_passed));

                    part_num = 0;
                    word.clear();
                    src_lang.clear();
                    dst_lang.clear();
                    tmp_str.clear();
                    continue;
                }

                ++part_num;
                tmp_str.clear();
                continue;
            }

            tmp_str += current_char;
        }
        return result;
    }
}
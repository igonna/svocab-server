#include "progress_words.h"

namespace core::progress_words {

std::unordered_map<std::string, UserWordData> get_user_words_from_str(const std::string& words_str) {
    std::unordered_map<std::string, UserWordData> result;

    auto len = words_str.size();

    uint32_t part_num = 0;
    std::string tmp_str;
    
    std::string word;
    uint32_t confirmation_level;
    bool is_hidden;
    uint32_t last_update_time;

    for (uint32_t i = 0; i < len; ++i) {
        auto current_char = words_str[i];

        if (current_char == ';') {
            if (part_num == 0) {
                word = std::move(tmp_str);
            } else if (part_num == 1) {
                confirmation_level = std::stoi(tmp_str);
            } else if (part_num == 2) {
                is_hidden = tmp_str[0] == '0' ? false : true;
            } else if (part_num == 3) {
                last_update_time = std::stoul(tmp_str);

                // add word
                result[word] = UserWordData(confirmation_level, is_hidden, last_update_time);
                part_num = 0;
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
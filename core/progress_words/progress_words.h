#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>
#include "utils/constants.h"

namespace core::progress_words {

static std::string create_hidden_word_str(const std::string& word, bool is_hidden, uint32_t time) {
    return word + ";0;" + (is_hidden ? '1': '0') + ";" + std::to_string(time) + ';';
}

static std::string create_learned_word_str(const std::string& word, uint32_t confirmation_level, uint32_t time) {
    return word + ";" + std::to_string(confirmation_level) + ";0;" + std::to_string(time) + ';';
}

inline static std::string create_user_word_str(const std::string& word, bool is_hidden, uint32_t confirmation_level, uint32_t time) {
    return word + ";" + std::to_string(confirmation_level) + ";" + (is_hidden ? '1': '0') + ";" + std::to_string(time) + ';';
}

struct UserWordData {
    uint32_t confirmation_level;
    bool is_hidden;
    uint32_t last_update_time;

    inline bool is_ready_for_learn(uint32_t time) {
        if (is_hidden) return false;

        int32_t seconds_passed = time - last_update_time;

        if (confirmation_level == 1) {
            return seconds_passed > local_constants::confirmation_level_1_repeat_timeout_seconds;
        } else if (confirmation_level == 2) {
            return seconds_passed > local_constants::confirmation_level_2_repeat_timeout_seconds;
        } else if (confirmation_level == 3) {
            return seconds_passed > local_constants::confirmation_level_3_repeat_timeout_seconds;
        } else {
            return false;
        }
    }
};

std::tuple<uint32_t, std::unordered_map<std::string, UserWordData>> get_user_words_from_str(const std::string& words_str);

std::string user_words_to_str(const std::unordered_map<std::string, UserWordData>& user_words);
}
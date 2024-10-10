#pragma once
#include <string>
#include <unordered_map>

namespace core::progress_words {

static std::string create_hidden_word_str(const std::string& word, bool is_hidden, uint32_t time) {
    return word + ";0;" + (is_hidden ? '1': '0') + ";" + std::to_string(time) + ';';
}

static std::string create_learned_word_str(const std::string& word, uint32_t confirmation_level, uint32_t time) {
    return word + ";" + std::to_string(confirmation_level) + ";0;" + std::to_string(time) + ';';
}

struct UserWordData {
    uint32_t confirmation_level;
    bool is_hidden;
    uint32_t last_update_time;
};

std::unordered_map<std::string, UserWordData> get_user_words_from_str(const std::string& words_str);
}
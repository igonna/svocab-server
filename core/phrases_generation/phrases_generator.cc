#include "phrases_generator.h"
#include "utils/utils.h"
#include "core/text_processing/text_processor.h"
#include <random>
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <unicode/uloc.h>
#include <unicode/unistr.h>

namespace core::phrases_generator {

icu::UnicodeString& swap_unicode_str_from_behind(icu::UnicodeString& unicode_str, uint32_t depth) {
    int32_t cur_index = unicode_str.length() - 1;
    int32_t swap_index = cur_index - 1;

    UChar32 tmp_char;

    while (depth > 0 && cur_index > 0 && swap_index > 0) {
        tmp_char = unicode_str[cur_index];
        unicode_str.setCharAt(cur_index, unicode_str[swap_index]);
        unicode_str.setCharAt(swap_index, tmp_char);
        --depth;
        --swap_index;
    }

    return unicode_str;
}

std::vector<std::string> get_guess_words(const std::string& base_word, const std::string& random_word) {
    auto base_word_unicode = core::text_processing::string_to_unicode(base_word);

    std::unordered_set<std::string> guess_unique_words;
    guess_unique_words.insert(base_word);

    constexpr uint32_t guess_words_count = 3;

    int32_t base_word_len = base_word_unicode.length();
    int32_t third_of_len = base_word_len / 3;

    for (uint32_t i = 0; i < guess_words_count - 1; ++i) {
        auto first_half = base_word_unicode.tempSubString(0, third_of_len);
        auto second_half = base_word_unicode.tempSubString(third_of_len, base_word_len - third_of_len);
        second_half = core::text_processing::shuffle_unicode_string(second_half);

        auto new_str = core::text_processing::unicode_to_str(first_half + second_half);
        if (!guess_unique_words.contains(new_str)) {
            guess_unique_words.insert(new_str);
            continue;
        }

        new_str = core::text_processing::unicode_to_str(swap_unicode_str_from_behind(base_word_unicode, 2));
        if (!guess_unique_words.contains(new_str)) {
            guess_unique_words.insert(new_str);
            continue;
        }

        auto second_half_len = second_half.length();
        auto random_word_unicode = core::text_processing::string_to_unicode(random_word);
        auto second_half_shuffled = core::text_processing::shuffle_unicode_string(second_half + random_word_unicode);

        new_str = core::text_processing::unicode_to_str(first_half + second_half_shuffled.tempSubString(0, second_half_len));
        if (!guess_unique_words.contains(new_str)) {
            guess_unique_words.insert(new_str);
            continue;
        }
    }

    std::vector<std::string> guess_words(guess_unique_words.begin(), guess_unique_words.end());

    // if failed to have different words add left
    while (guess_words.size() != guess_words_count) {
        guess_words.push_back(core::text_processing::shuffle_string(base_word));
    }

    // shuffle guess_words
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(guess_words.begin(), guess_words.end(), rd);

    return guess_words;
}

drogon::Task<FocusGroupPhrase> PhrasesGenerator::generate_phrase_with_words_and_letters(core::user_sequence_words::SequenceUserWord &user_word, bool user_has_subscription) {
    auto generated_phrase = co_await openai_phrase_generation->generate_phrase(user_word.src_lang, user_word.dst_lang, user_word.word, user_has_subscription);

    auto word_phrase_table_name = local_utils::translations_db::create_word_phrase_table_name(user_word.src_lang);
    auto query = std::string("select MAX(id) as max_id from " + word_phrase_table_name + " where dst_lang = $1");

    auto max_word_phrase_table_id = co_await translations_db->execSqlCoro(query, user_word.dst_lang);

    constexpr uint32_t min_num = 1;
    auto max_num = max_word_phrase_table_id[0]["max_id"].as<uint32_t>();

    // get random word from db
    auto random_word_id = local_utils::random::get_random_number(min_num, max_num);
    auto random_word_result = co_await translations_db->execSqlCoro("select word from " + word_phrase_table_name + " where dst_lang = $1 and id = $2 limit 1", user_word.dst_lang, random_word_id);

    std::string random_word = random_word_result.size() ? random_word_result[0]["word"].as<std::string>() : user_word.word;

    // add guess letters
    std::vector<std::string> guess_letters;
    {
        auto letters = core::text_processing::extract_letters_as_strings(user_word.word, user_word.src_lang.c_str());

        guess_letters.insert(guess_letters.end(), 
            std::make_move_iterator(letters.begin()), 
            std::make_move_iterator(letters.end())
        );

        auto random_word_letters = core::text_processing::extract_letters_as_strings(random_word, user_word.src_lang.c_str());

        constexpr uint32_t guess_letters_to_add_from_random_word = 3;
        uint32_t letters_added = 0;

        for (auto& letter : random_word_letters) {
            if (++letters_added > guess_letters_to_add_from_random_word) break;
            guess_letters.push_back(std::move(letter));
        }
    }

    // add guess words
    auto guess_words = get_guess_words(user_word.word, random_word);

    // shuffle guess letters
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(guess_letters.begin(), guess_letters.end(), rd);

    co_return FocusGroupPhrase(std::move(generated_phrase), std::move(guess_words), std::move(guess_letters), user_word.confirmation_level);
}

void PhrasesGenerator::add_phrase(core::user_sequence_words::SequenceUserWord &user_word, bool user_has_subscription) {
    auto current_generation_task = generate_phrase_with_words_and_letters(user_word, user_has_subscription);
    auto generation_pair = std::make_pair(&user_word, std::move(current_generation_task));
    phrases_to_generate.push_back(std::move(generation_pair));
}

drogon::Task<std::vector<FocusGroupPhrase>> PhrasesGenerator::get_generated_phrases() {
    std::vector<FocusGroupPhrase> generated_phrases;

    for (auto& [user_word_ref, generation_task] : phrases_to_generate) {
        try {
            auto generated_phrase = co_await generation_task;
            generated_phrases.push_back(std::move(generated_phrase));
        } catch (const OpenaiPhraseGeneration::word_not_valid_exception& e) {
            user_word_ref->is_passed = true;
        }
    }

    phrases_to_generate.clear();

    co_return generated_phrases;
}

uint32_t PhrasesGenerator::count() const {
    return phrases_to_generate.size();
}
};
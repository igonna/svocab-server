#include "phrases_generator.h"
#include "utils/utils.h"
#include "core/text_processing/text_processor.h"
#include <random>

namespace core::phrases_generator {

drogon::Task<FocusGroupPhrase> PhrasesGenerator::generate_phrase_with_words_and_letters(core::user_sequence_words::SequenceUserWord &user_word, bool user_has_subscription) {
    auto generated_phrase = co_await openai_phrase_generation->generate_phrase(user_word.src_lang, user_word.dst_lang, user_word.word, user_has_subscription);

    auto word_phrase_table_name = local_utils::translations_db::create_word_phrase_table_name(user_word.src_lang);
    auto query = std::string("select MAX(id) as max_id from " + word_phrase_table_name + " where dst_lang = $1");

    auto max_word_phrase_table_id = co_await translations_db->execSqlCoro(query, user_word.dst_lang);

    constexpr uint32_t min_num = 1;
    auto max_num = max_word_phrase_table_id[0]["max_id"].as<uint32_t>();

    std::vector<std::string> guess_words;
    std::vector<std::string> guess_letters;

    guess_words.push_back(user_word.word);

    // try to add all different words
    constexpr uint32_t guess_words_count = 3;
    std::unordered_set<std::string> unique_words;
    unique_words.insert(user_word.word);

    constexpr uint32_t max_retries = 20;
    uint32_t retries = 0;

    while (guess_words.size() < guess_words_count && retries < max_retries) {
        std::string shuffled_word = core::text_processing::shuffle_string_in_the_middle(user_word.word, user_word.src_lang.c_str());
        if (!unique_words.contains(shuffled_word)) {
            guess_words.push_back(shuffled_word);
            unique_words.insert(shuffled_word);
        }
        ++retries;
    }

    // if failed to have different words add left
    while (guess_words.size() != guess_words_count) {
        guess_words.push_back(core::text_processing::shuffle_string(user_word.word, user_word.src_lang.c_str()));
    }

    auto letters = core::text_processing::extract_letters_as_strings(user_word.word, user_word.src_lang.c_str());

    guess_letters.insert(guess_letters.end(), 
        std::make_move_iterator(letters.begin()), 
        std::make_move_iterator(letters.end())
    );

    {
        auto random_word_nums = local_utils::random::get_random_nums_in_range(min_num, max_num, 1);
        auto random_word_id = random_word_nums.size() ? *random_word_nums.begin() : 0;

        // get word by id from db
        auto random_word_result = co_await translations_db->execSqlCoro(
            "select word from " + word_phrase_table_name + " where dst_lang = $1 and id = $2 limit 1",
            user_word.dst_lang,
            random_word_id
        );

        if (random_word_result.size()) {
            auto random_word = random_word_result[0]["word"].as<std::string>();

            auto random_word_letters = core::text_processing::extract_letters_as_strings(random_word, user_word.src_lang.c_str());

            constexpr uint32_t guess_letters_to_add_from_random_word = 3;
            uint32_t letters_added = 0;

            for (auto& letter : random_word_letters) {
                if (++letters_added > guess_letters_to_add_from_random_word) break;
                guess_letters.push_back(std::move(letter));
            }
        }
    }

    // shuffle guess_words
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(guess_words.begin(), guess_words.end(), rd);

    // shuffle guess letters
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
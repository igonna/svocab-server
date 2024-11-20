#include "phrases_generator.h"

namespace core::phrases_generator {

void PhrasesGenerator::add_phrase(core::user_sequence_words::SequenceUserWord &user_word, bool user_has_subscription) {        
    auto current_generation_task = openai_phrase_generation->generate_phrase(user_word.src_lang, user_word.dst_lang, user_word.word, user_has_subscription);
    auto generation_pair = std::make_pair(&user_word, std::move(current_generation_task));
    phrases_to_generate.push_back(std::move(generation_pair));
}

// could throw systems_are_busy_exception
// if word not valid sets: user_word_ref->is_excluded = true
drogon::Task<std::vector<GeneratedPhrase>> PhrasesGenerator::get_generated_phrases() {
    std::vector<GeneratedPhrase> generated_phrases;

    for (auto& [user_word_ref, generation_task] : phrases_to_generate) {
        try {
            auto generated_phrase = co_await generation_task;
            generated_phrases.push_back(std::move(generated_phrase));
        } catch (const OpenaiPhraseGeneration::word_not_valid_exception& e) {
            user_word_ref->is_excluded = true;
        }
    }

    phrases_to_generate.clear();

    co_return generated_phrases;
}

uint32_t PhrasesGenerator::count() const {
    return phrases_to_generate.size();
}
};
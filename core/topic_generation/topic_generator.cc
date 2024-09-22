#include "topic_generator.h"
#include "core/text_processing/text_processor.h"
#include "core/topic_generation/topic_generator.h"
#include <map>

namespace core::topic_generation {

TopicGenerator::TopicGenerator(
    std::string src_lang,
    float skip_value,
    OrderWordsBy order_words_by,
    bool exclude_word_if_not_present_in_all_sources,
    bool exclude_word_if_very_rare,
    bool exclude_word_if_word_learned
) : 
    _src_lang(src_lang),
    _skip_value(skip_value),
    _order_words_by(order_words_by),
    _exclude_word_if_not_present_in_all_sources(exclude_word_if_not_present_in_all_sources),
    _exclude_word_if_very_rare(exclude_word_if_very_rare),
    _exclude_word_if_word_learned(exclude_word_if_word_learned) {}


void TopicGenerator::add_topic_words(const std::vector<core::text_processing::ProcessedWord>& words) {
    auto words_count = words.size();

    for (auto& word : words) {
        auto word_data = WordData();

        if (_order_words_by == topic_generation::TopicGenerator::OrderWordsBy::total_popularity) {
            word_data.popularity_value = static_cast<float>(word.total_count);
        } else if (_order_words_by == topic_generation::TopicGenerator::OrderWordsBy::relative_popularity) {
            word_data.popularity_value = static_cast<float>(word.total_count) / words_count;
        }

        word_data.first_appearance = word.first_appearance;
        word_data.sources_count = 1;

        if (sources_words.contains(word.word)) {
            auto& ref = sources_words.at(word.word);
            ref.first_appearance = ref.first_appearance > word_data.first_appearance ? ref.first_appearance : word_data.first_appearance;
            ref.popularity_value += word_data.popularity_value;
            ref.sources_count += 1;
        } else {
            sources_words[word.word] = word_data;
        }
    }

    ++sources_count;
}

std::string TopicGenerator::get_topic_sequence_as_string() {
    OrderWordsComparator<float> order_words_comparator(_order_words_by);
    std::multimap<float, std::string, OrderWordsComparator<float>> ordered_words(order_words_comparator);

    for (auto& word : sources_words) {
        if (_exclude_word_if_not_present_in_all_sources && word.second.sources_count != sources_count) {
            continue;
        }

        if (topic_generation::TopicGenerator::OrderWordsBy::first_appearance == _order_words_by) {
            ordered_words.insert({static_cast<float>(word.second.first_appearance), word.first});
        } else {
            ordered_words.insert({static_cast<float>(word.second.popularity_value), word.first});
        }
    }

    uint32_t skip_words_count = _skip_value * ordered_words.size();
    uint32_t skipped_words = 0;

    std::string result;
    for (auto& i : ordered_words) {
        if (++skipped_words <= skip_words_count) continue;
        result.append(i.second + ';');
    }

    result.pop_back();

    return result;
}

}
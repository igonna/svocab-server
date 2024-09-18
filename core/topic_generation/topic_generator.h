#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <core/text_processing/text_processor.h>


namespace core::topic_generation {

class TopicGenerator {
public:
    enum class OrderWordsBy : uint32_t {first_appearance, relative_popularity, total_popularity};

    TopicGenerator(
        std::string src_lang,
        float skip_value,
        OrderWordsBy order_words_by,
        bool exclude_word_if_not_present_in_all_sources,
        bool exclude_word_if_very_rare
    ); 

    void add_topic_words(const std::vector<core::text_processing::ProcessedWord>& words);

    std::string get_topic_sequence_as_string();

    TopicGenerator() = delete;
    TopicGenerator(const TopicGenerator&) = delete;
    TopicGenerator(TopicGenerator&&) = delete;
    TopicGenerator& operator=(const TopicGenerator&) = delete;
    TopicGenerator& operator=(TopicGenerator&&) = delete;

private:
    std::string _src_lang;
    float _skip_value;
    OrderWordsBy _order_words_by;
    bool _exclude_word_if_not_present_in_all_sources;
    bool _exclude_word_if_very_rare;

private:
    struct WordData {
        uint32_t sources_count;
        float popularity_value;
        uint32_t first_appearance;
    };

    template <typename T>
    struct OrderWordsComparator {
        bool use_greater;

        OrderWordsComparator(topic_generation::TopicGenerator::OrderWordsBy order_words_by) 
            : use_greater(topic_generation::TopicGenerator::OrderWordsBy::first_appearance != order_words_by) {}

        bool operator()(const T& lhs, const T& rhs) const {
            if (use_greater) {
                return std::greater<T>()(lhs, rhs);
            } else {
                return std::less<T>()(lhs, rhs);
            }
        }
    };

private:
    uint32_t sources_count = 0;
    const uint32_t very_rare_words_limit = 10000;
    std::unordered_map<std::string, WordData> sources_words;
};

}

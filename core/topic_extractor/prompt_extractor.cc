#include "topic_extractor_factory.h"
#include "utils/utils.h"
#include "utils/exceptions.h"
#include "core/text_extraction/html_extractor.h"
#include "core/text_processing/text_processor.h"
#include "core/website_fetch/website_fetch.h"
#include "plugins/FrequentWords/FrequentWords.h"
#include "core/postgres/queries.h"
#include "plugins/OpenaiGeneration/OpenaiGeneration.h"

namespace core::topic_extractor {

using namespace drogon;

drogon::Task<> PromptExtractor::init(drogon::HttpRequestPtr req) {
    const auto prompt = std::string(req->getBody());

    if (prompt.empty()) {
        throw exception::not_valid_body();
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto user_has_subscription = co_await core::sql::does_user_have_subscription(users_db, user_id);

        auto [limit_reached, limit_resets_time] = co_await core::sql::user_limits::is_prompt_generated_topic_limit_reached(
            users_db, user_id, user_has_subscription
        );

        if (limit_reached) {
            throw exception::prompt_topic_generation_limit_reached(std::to_string(limit_resets_time));
        }

        auto src_lang = req->getHeader("srcLang");
        auto dst_lang = req->getHeader("dstLang");

        auto openai_generation_plugin = app().getPlugin<OpenaiGeneration>();
        auto text = co_await openai_generation_plugin->generate_words_list(src_lang, prompt, user_has_subscription);

        auto frequent_words_plugin = app().getPlugin<FrequentWords>();
        auto prioritized_words_sequence = core::text_processing::create_prioritized_words_sequence(text, frequent_words_plugin, src_lang.c_str(), dst_lang.c_str());

        if (prioritized_words_sequence.empty()) throw exception::data_extract_fail();

        title = local_utils::strings::get_title_from_str(prompt, local_constants::title_max_length);
        origin = prompt;
        origin_type = "prompt";
        words_sequence = std::move(prioritized_words_sequence);

        co_await core::sql::user_limits::increase_prompt_generated_topic_count(users_db, user_id, 1);

    } catch (const OpenaiGeneration::systems_are_busy_exception& e) {
        throw exception::systems_are_busy();
    }

    co_return;
}

}
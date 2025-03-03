#include "topic_extractor_factory.h"
#include "utils/utils.h"
#include "utils/exceptions.h"
#include "core/text_extraction/html_extractor.h"
#include "core/text_processing/text_processor.h"
#include "core/website_fetch/website_fetch.h"
#include "plugins/FrequentWords/FrequentWords.h"

namespace core::topic_extractor {

using namespace drogon;

drogon::Task<> HtmlExtractor::init(drogon::HttpRequestPtr req) {
    auto html_content = std::string(req->getBody());
    origin = req->getHeader("origin");

    if (html_content.empty()) {
        throw exception::not_valid_body();
    }

    if (origin.empty()) {
        throw exception::not_valid_header();
    }

    try {
        std::tie(title, html_content) = core::text_extraction::extract_text_and_title_from_html(html_content);

        if (html_content.empty()) throw exception::data_extract_fail();

        auto src_lang = req->getHeader("srcLang");
        auto dst_lang = req->getHeader("dstLang");

        auto frequent_words_plugin = app().getPlugin<FrequentWords>();
        auto prioritized_words_sequence = core::text_processing::create_prioritized_words_sequence(html_content, frequent_words_plugin, src_lang.c_str(), dst_lang.c_str());

        if (prioritized_words_sequence.empty()) throw exception::data_extract_fail();

        title = local_utils::strings::get_title_from_str(title, local_constants::title_max_length);
        origin_type = "html";
        words_sequence = std::move(prioritized_words_sequence);

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        throw exception::data_extract_fail();
    }

    co_return;
}

}
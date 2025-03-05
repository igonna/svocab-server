#include "topic_extractor_factory.h"
#include "utils/utils.h"
#include "utils/exceptions.h"
#include "core/text_extraction/html_extractor.h"
#include "core/text_processing/text_processor.h"
#include "core/website_fetch/website_fetch.h"
#include "plugins/FrequentWords/FrequentWords.h"

namespace core::topic_extractor {

using namespace drogon;

drogon::Task<> LinkExtractor::init(drogon::HttpRequestPtr req) {
    const auto url = std::string(req->getBody());

    if (url.empty()) {
        throw exception::not_valid_body();
    }

    try {
        core::website_fetch::CurlFetchWebsite website_fetcher;
        std::string website_body = website_fetcher.fetch_html(url);

        std::tie(title, website_body) = core::text_extraction::extract_text_and_title_from_html(website_body);

        if (website_body.empty()) throw exception::data_extract_fail();

        auto src_lang = req->getHeader("srcLang");
        auto dst_lang = req->getHeader("dstLang");

        auto frequent_words_plugin = app().getPlugin<FrequentWords>();
        auto prioritized_words_sequence = core::text_processing::create_prioritized_words_sequence(website_body, frequent_words_plugin, src_lang.c_str(), dst_lang.c_str());

        if (prioritized_words_sequence.empty()) throw exception::data_extract_fail();

        title = local_utils::strings::get_title_from_str(title, local_constants::title_max_length);
        origin = std::move(url);
        origin_type = "link";
        words_sequence = std::move(prioritized_words_sequence);

    } catch (const std::exception& e) {
        LOG_ERROR << url << " " << e.what();
        throw exception::data_extract_fail();
    }

    co_return;
}

}
#include "topic_extractor_factory.h"
#include "utils/utils.h"
#include "utils/exceptions.h"
#include "core/text_extraction/html_extractor.h"
#include "core/text_processing/text_processor.h"

namespace core::topic_extractor {

using namespace drogon;

std::string extract_text_from_html(const std::string& html) {

}

drogon::Task<> LinkExtractor::init(drogon::HttpRequestPtr req) try {
    const auto& url = req->getBody();

    if (!url.size()) {
        throw exception::not_valid_body();
    }

    auto [host, path] = local_utils::url::extract_host_and_path_from_url(url);

    auto client = HttpClient::newHttpClient(host);
    auto website_request = HttpRequest::newHttpRequest();
    website_request->addHeader("User-Agent", "Mozilla/5.0 (X11; Ubuntu; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");
    website_request->setPath(path);
    website_request->setMethod(Get);

    drogon::HttpResponsePtr website_resp = co_await client->sendRequestCoro(website_request);

    if (website_resp->getStatusCode() != k200OK) throw exception::data_extract_fail();

    auto website_body = std::string(website_resp->getBody());
    std::tie(title, website_body) = core::text_extraction::extract_text_and_title_from_html(website_body);

    if (website_body.empty()) throw exception::data_extract_fail();

    title = local_utils::strings::get_title_from_str(title, local_constants::title_max_length);
    origin = url;
    origin_type = "link";
    // words_sequence = ;

    std::string processed_words = core::text_processing::text_to_processed_source(std::move(unicode_str), topic_src_lang.c_str());

    if (title.empty()) {
        title = local_utils::strings::get_title_from_str(words_sequence, local_constants::title_max_length);
    }
} catch (const std::exception& e) {
    throw exception::data_extract_fail();
}

}
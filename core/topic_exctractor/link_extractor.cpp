#include "topic_extractor_factory.h"
#include "utils/utils.h"

namespace core::topic_extractor {

drogon::Task<> LinkExtractor::init(drogon::HttpRequestPtr req) {
    const auto& url = req->getBody();

    if (!url.size()) {
        throw exception::not_valid_body();
    }

    try {
        auto [host, path] = local_utils::url::extract_host_and_path_from_url(url);

        // fetch website content
        auto client = HttpClient::newHttpClient(host);
        auto website_request = HttpRequest::newHttpRequest();
        website_request->addHeader("User-Agent", "Mozilla/5.0 (");
        website_request->setPath(path);
        website_request->setMethod(Get);

        bool failed_to_reach_website = false;
        drogon::HttpResponsePtr website_resp;

        try {
            website_resp = co_await client->sendRequestCoro(website_request);
        } catch (const std::exception& e) {
            LOG_ERROR << e.what();
            failed_to_reach_website = true;
        }

        if (failed_to_reach_website || website_resp->getStatusCode() != k200OK) {
            fail_type = "failed_to_reach_website";
        } else {
            auto website_body = std::string(website_resp->getBody());

            std::tie(title, text) = core::text_extraction::extract_text_and_title_from_html(website_body);
            if (text.empty()) {
                text = std::move(website_body);
                website_body.clear();
            }

            title = local_utils::strings::get_title_from_str(title, local_constants::title_max_length);

            if (title.empty()) {
                title = local_utils::strings::get_title_from_str(text, local_constants::title_max_length);
            }
        }

        if ((title.empty() || text.empty()) && fail_type.empty()) {
            fail_type = "failed_to_fetch_content";
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    const std::string source_type = "website_url";
    uint32_t source_id;
    bool topic_title_updated = false;

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);
        
        std::string processed_words;
        if (fail_type.empty()) {
            auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

            auto unicode_str = core::text_processing::string_to_unicode(text);
            processed_words = core::text_processing::text_to_processed_source(std::move(unicode_str), topic_src_lang.c_str());
        
            if (processed_words.empty()) {
                fail_type = "no_words_found";
            }
        }

        source_id = co_await core::sql::add_topic_source(users_db, user_id, topic_id, title, source_type, processed_words, "", fail_type);
        topic_title_updated = co_await update_user_topic_title_if_empty(users_db, title, user_id, topic_id);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    auto result_body = createTopicGenerationItem(source_id, title, source_type, fail_type, topic_title_updated);
    co_return local_utils::Response::JSON_OK(callback, result_body);
}

}
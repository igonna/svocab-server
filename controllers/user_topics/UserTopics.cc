#include "UserTopics.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/text_extraction/html_extractor.h"
#include "core/text_extraction/pdf_extractor.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"
#include "core/text_processing/text_processor.h"

namespace v1
{
Task<> UserTopics::enableFirstUserTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    co_return;
}

Task<> UserTopics::createTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto src_lang = req->getHeader("srcLang");
    auto dst_lang = req->getHeader("dstLang");

    if (!(src_lang.size() && dst_lang.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    Json::Value resultTopic;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        uint32_t position = 0;

        auto result = co_await users_db->execSqlCoro("insert into user_topics (user_id, src_lang, dst_lang, position) "
            "values ($1, $2, $3, $4) returning "
            "topic_id, src_lang, topic_title, count_word_as, order_words_by, "
            "exclude_word_if_not_present_in_all_sources, exclude_word_if_very_rare, "
            "texts_titles, texts_fail_title_status, "
            "documents_titles, documents_fail_title_status, "
            "website_urls_titles, website_urls_fail_title_status, "
            "youtube_urls_titles, youtube_urls_fail_title_status"
            , user_id, src_lang, dst_lang, position
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("topic_not_created");
        }

        resultTopic = createTopicGenerationWindow(result[0]);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, resultTopic);
}

Task<> UserTopics::deleteTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");

    if (!topic_id_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        co_await users_db->execSqlCoro("delete from user_topics where user_id = $1 and topic_id = $2", user_id, std::stoull(topic_id_str));

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::getUserTopics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    co_return;
}

Task<> UserTopics::getTopicGenerationWindow(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id = req->getHeader("topicId");

    if (!topic_id.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    Json::Value result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro(
            "select "
            "topic_id, src_lang, topic_title, count_word_as, order_words_by, "
            "exclude_word_if_not_present_in_all_sources, exclude_word_if_very_rare, "
            "texts_titles, texts_fail_title_status, "
            "documents_titles, documents_fail_title_status, "
            "website_urls_titles, website_urls_fail_title_status, "
            "youtube_urls_titles, youtube_urls_fail_title_status "
            "from user_topics where user_id = $1 and topic_id = $2"
            , user_id, topic_id
        );

        if (!result.size()) {
            throw std::runtime_error("topic_not_exists");
        }

        result_body = createTopicGenerationWindow(result[0]);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Task<> UserTopics::setTopicTitle(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto topic_title = req->getHeader("topicTitle").substr(0, local_constants::title_max_length);

    if (!topic_id_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("update user_topics set topic_title = $1 where user_id = $2 and topic_id = $3", topic_title, user_id, std::stoull(topic_id_str));
        if (!result.affectedRows()) {
            throw std::runtime_error("not_updated");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::addTopicSourceText(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto text = std::string(req->getBody());

    if (!(topic_id_str.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    if (!text.size()) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "body_is_empty");
    }

    auto title = text.substr(0, local_constants::title_max_length);

    std::string processed_words;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoull(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        processed_words = core::text_processing::text_to_processed_words(std::move(unicode_str), topic_src_lang.c_str());

        auto result = co_await users_db->execSqlCoro(
            "update user_topics set "
            "texts_sources = array_append(texts_sources, $1), "
            "texts_processed = array_append(texts_processed, $2), "
            "texts_titles =  array_append(texts_titles, $3), "
            "last_update_id = last_update_id + 1 "
            "where user_id = $4 and topic_id = $5",
            text, processed_words, title, user_id, topic_id
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("can_not_update");
        }

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);        
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, processed_words);
}

Task<> UserTopics::addTopicSourceDocument(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");

    MultiPartParser multiPartParser;
    if (multiPartParser.parse(req) != 0) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "empty");
    }

    auto &files = multiPartParser.getFiles();

    if (!files.size()) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "no_files");
    }

    auto file = files[0];
    auto content_type = file.getContentType();

    std::string title;
    std::string text;

    if (content_type == drogon::ContentType::CT_TEXT_PLAIN) {
        text = file.fileContent();
        title = file.getFileName().substr(0, local_constants::title_max_length);
    } else if (content_type == drogon::ContentType::CT_APPLICATION_PDF) {
        text = core::text_extraction::get_text_from_pdf(file.fileData(), file.fileLength());
        title = file.getFileName().substr(0, local_constants::title_max_length);
    } else if (content_type == drogon::ContentType::CT_TEXT_HTML) {
        auto content = std::string(file.fileContent());
        std::tie(title, text) = core::text_extraction::extract_text_and_title_from_html(content);
        title = title.substr(0, local_constants::title_max_length);
    } else {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "not_supported_content_type");
    }

    std::string processed_words;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoull(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        processed_words = core::text_processing::text_to_processed_words(std::move(unicode_str), topic_src_lang.c_str());

        auto result = co_await users_db->execSqlCoro(
            "update user_topics set "
            "documents_processed = array_append(documents_processed, $1), "
            "documents_titles =  array_append(documents_titles, $2), "
            "last_update_id = last_update_id + 1 "
            "where user_id = $3 and topic_id = $4",
            processed_words, title, user_id, topic_id
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("can_not_update");
        }

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);        
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, processed_words);
}

Task<> UserTopics::addTopicSourceWebsiteUrl(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{   
    auto topic_id_str = req->getHeader("topicId"); 
    auto url = req->getHeader("url");

    if (!url.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    std::string title;
    std::string text;

    try {
        auto [host, path] = local_utils::url::extract_host_and_path_from_url(url);

        // fetch website content
        auto client = HttpClient::newHttpClient(host);
        auto website_request = HttpRequest::newHttpRequest();
        website_request->addHeader("User-Agent", "Mozilla/5.0 (");
        website_request->setPath(path);
        website_request->setMethod(Get);
        auto website_resp = co_await client->sendRequestCoro(website_request);

        if (website_resp->getStatusCode() != k200OK) {
            co_return local_utils::Response::Error::FORBIDDEN(callback, "can_not_fetch_content");
        }

        auto website_html = std::string(website_resp->getBody());
        std::tie(title, text) = core::text_extraction::extract_text_and_title_from_html(website_html);
        title = title.substr(0, local_constants::title_max_length);

        if (!(title.size() && text.size())) {
            co_return local_utils::Response::Error::FORBIDDEN(callback, "can_not_fetch_content");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    std::string processed_words;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoull(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        processed_words = core::text_processing::text_to_processed_words(std::move(unicode_str), topic_src_lang.c_str());

        auto result = co_await users_db->execSqlCoro(
            "update user_topics set "
            "website_urls_processed = array_append(website_urls_processed, $1), "
            "website_urls_titles =  array_append(website_urls_titles, $2), "
            "last_update_id = last_update_id + 1 "
            "where user_id = $3 and topic_id = $4",
            processed_words, title, user_id, topic_id
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("can_not_update");
        }

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);        
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, processed_words);
}

Task<> UserTopics::addTopicSourceYoutubeUrl(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId"); 
    auto title = req->getHeader("title").substr(0, local_constants::title_max_length);;
    auto text = std::string(req->getBody());

    if (!title.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    if (!text.size()) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "body_is_empty");
    }

    std::string processed_words;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoull(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        processed_words = core::text_processing::text_to_processed_words(std::move(unicode_str), topic_src_lang.c_str());

        auto result = co_await users_db->execSqlCoro(
            "update user_topics set "
            "youtube_urls_processed = array_append(youtube_urls_processed, $1), "
            "youtube_urls_titles = array_append(youtube_urls_titles, $2), "
            "last_update_id = last_update_id + 1 "
            "where user_id = $3 and topic_id = $4",
            processed_words, title, user_id, topic_id
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("can_not_update");
        }

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);        
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, processed_words);
}


Task<> UserTopics::deleteTopicSource(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    co_return;
}

Task<> UserTopics::deleteTopicFailedSource(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    co_return;
}

Task<> UserTopics::getTopicSourceTextContent(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    co_return;
}

Task<> UserTopics::setTopicSourceText(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    co_return;
}

Task<> UserTopics::setTopicOption(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    co_return;
}

Task<> UserTopics::setTopicEnabled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto value = req->getHeader("value");

    if (!topic_id_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("update user_topics set is_enabled = $1 where user_id = $2 and topic_id = $3", value, user_id, std::stoull(topic_id_str));
        if (!result.affectedRows()) {
            throw std::runtime_error("not_updated");
        }

        // update sequence last_update_id
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::setTopicPosition(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    co_return;
}

Task<> UserTopics::getTopicWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");

    if (!topic_id_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    std::string result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("select topic_processed_words from user_topics where user_id = $1 and topic_id = $2", user_id, std::stoull(topic_id_str));
        if (!result.size()) {
            throw std::runtime_error("not_exists");
        }

        result_body = result[0]["topic_processed_words"].as<std::string>();
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, result_body);
}


Json::Value UserTopics::createTopicGenerationWindow(const drogon::orm::Result::Reference& row)
{
    Json::Value result;
    result["topic_id"] = row["topic_id"].as<uint32_t>();

    result["src_lang"] = row["src_lang"].as<std::string>();

    result["topic_title"] = row["topic_title"].as<std::string>();

    auto& options = result["options"];
    options["count_word_as"] = row["count_word_as"].as<uint32_t>();
    options["order_words_by"] = row["order_words_by"].as<uint32_t>();
    options["exclude_word_if_not_present_in_all_sources"] = row["exclude_word_if_not_present_in_all_sources"].as<bool>();
    options["exclude_word_if_very_rare"] = row["exclude_word_if_very_rare"].as<bool>();

    auto& sources = result["sources"] = Json::arrayValue;

    static auto new_source = [](const std::string& title, std::string from, std::string type) {
        Json::Value source;
        source["title"] = title;
        source["from"] = from; // text, document, website, youtube
        source["type"] = type; // source, fail
        return source;
    };

    // text
    auto texts_titles = local_utils::postgres::parse_array_string(row["texts_titles"].as<std::string>());

    for (auto& i : texts_titles) {
        sources.append(new_source(i, "text", "source"));
    }

    auto texts_fail_title_status = row["texts_fail_title_status"].asArray<std::string>();
    for (auto& i : texts_fail_title_status) {
        sources.append(new_source(*i, "text", "fail"));
    }
    
    // document
    auto documents_titles = local_utils::postgres::parse_array_string(row["documents_titles"].as<std::string>());
    for (auto& i : documents_titles) {
        sources.append(new_source(i, "document", "source"));
    }

    auto documents_fail_title_status = row["documents_fail_title_status"].asArray<std::string>();
    for (auto& i : documents_fail_title_status) {
        sources.append(new_source(*i, "document", "fail"));
    }
    
    // website
    auto website_urls_titles = local_utils::postgres::parse_array_string(row["website_urls_titles"].as<std::string>());
    for (auto& i : website_urls_titles) {
        sources.append(new_source(i, "website_url", "source"));
    }

    auto website_urls_fail_title_status = row["website_urls_fail_title_status"].asArray<std::string>();
    for (auto& i : website_urls_fail_title_status) {
        sources.append(new_source(*i, "website_url", "fail"));
    }
    
    auto youtube_urls_titles = local_utils::postgres::parse_array_string(row["youtube_urls_titles"].as<std::string>());
    for (auto& i : youtube_urls_titles) {
        sources.append(new_source(i, "youtube_url", "source"));
    }
    
    auto youtube_urls_fail_title_status = row["youtube_urls_fail_title_status"].asArray<std::string>();
    for (auto& i : youtube_urls_fail_title_status) {
        sources.append(new_source(*i, "youtube_url", "fail"));
    }

    return result;
}
}
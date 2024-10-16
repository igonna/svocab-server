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
Task<> UserTopics::getFirstUserGlobalTopicId(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    std::string topic_id_str;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("select src_lang from user_language_pairs "
            "where user_id = $1 limit 1", user_id
        );

        if (result.empty()) {
            co_return local_utils::Response::Error::ERROR(callback, k404NotFound, "");
        }

        auto src_lang = result[0]["src_lang"].as<std::string>();

        result = co_await users_db->execSqlCoro("select topic_id from global_topics "
            "where src_lang = $1 limit 1", src_lang
        );

        if (result.empty()) {
            co_return local_utils::Response::Error::ERROR(callback, k404NotFound, "");
        }
        
        auto topic_id = result[0]["topic_id"].as<uint32_t>();
        topic_id_str = local_utils::topics::encode_topic_id(topic_id, local_utils::topics::TopicType::global_topic);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, topic_id_str);
}

Task<> UserTopics::createTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto src_lang = req->getHeader("srcLang");
    auto dst_lang = req->getHeader("dstLang");
    auto to_translate_lang = req->getHeader("toTranslateLang");

    if (!(src_lang.size() && dst_lang.size()) || to_translate_lang.size() != local_constants::languages::lang_tag_size) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    Json::Value result_topic;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("select 1 from user_language_pairs "
            "where user_id = $1 and src_lang = $2 and dst_lang = $3 limit 1", user_id, src_lang, dst_lang
        );

        if (result.empty()) {
            co_return local_utils::Response::Error::ERROR(callback, k404NotFound, "language_pair_not_exists");
        }

        auto new_position = co_await core::sql::get_new_smallest_topic_position(users_db, user_id, src_lang, dst_lang);

        result = co_await users_db->execSqlCoro("insert into user_topics (user_id, src_lang, dst_lang, position) "
            "values ($1, $2, $3, $4) returning "
            "topic_id, src_lang, topic_title, order_words_by, "
            "exclude_word_if_not_present_in_all_sources, exclude_word_if_very_rare, "
            "sources_metadata, sources_unprocessed, sources_processed"
            , user_id, src_lang, dst_lang, new_position
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("topic_not_created");
        }

        result_topic = createTopicGenerationWindow(result[0], to_translate_lang);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_topic);
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

        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);

        co_await users_db->execSqlCoro("delete from user_topics where user_id = $1 and topic_id = $2", user_id, topic_id);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::getUserTopics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto to_translate_lang = req->getHeader("toTranslateLang");

    if (to_translate_lang.size() != local_constants::languages::lang_tag_size) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    Json::Value result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto lang_pairs_result = co_await users_db->execSqlCoro(
            " select src_lang, dst_lang, mix_value"
            " from user_language_pairs"
            " where user_id = $1 and is_enabled = true order by last_enable_time desc", user_id
        );

        auto topics_result = co_await users_db->execSqlCoro(
            "( "
                "select "
                "ut.topic_id,"
                "ulp.src_lang,"
                "ulp.dst_lang,"
                "ulp.mix_value as lang_mix_value,"
                "ut.topic_title,"
                "ut.order_words_by,"
                "ut.mix_value,"
                "ut.skip_value,"
                "ut.is_enabled,"
                "ut.position,"
                "0 as topic_type,"
                "ulp.last_enable_time as ulp_last_enable_time "
                "from user_language_pairs as ulp "
                "left join user_topics as ut "
                "on ut.user_id = ulp.user_id "
                "and ut.src_lang = ulp.src_lang "
                "and ut.dst_lang = ulp.dst_lang "
                "where ulp.is_enabled = true "
                "and ulp.user_id = $1 "
                "and topic_id is not null "
            ") "
            "UNION "
                "( "
                "select "
                "gt.topic_id,"
                "ulp.src_lang,"
                "ulp.dst_lang,"
                "ulp.mix_value as lang_mix_value,"
                "gt.src_title_en as topic_title,"
                "null as order_words_by,"
                "COALESCE(ugt.mix_value, 1) as mix_value,"
                "COALESCE(ugt.skip_value, 0) as skip_value,"
                "COALESCE(ugt.is_enabled, false) as is_enabled,"
                "gt.default_position as position,"
                "1 as topic_type,"
                "ulp.last_enable_time as ulp_last_enable_time "
                "from user_language_pairs as ulp "
                "left join global_topics as gt "
                "on gt.src_lang = ulp.src_lang "
                "left join user_global_topics as ugt "
                "on gt.topic_id = ugt.topic_id "
                "and gt.src_lang = ugt.src_lang "
                "and ugt.dst_lang = ulp.dst_lang "
                "and ugt.user_id = ulp.user_id "
                "where ulp.is_enabled = true "
                "and ulp.user_id = $1 "
                "and gt.topic_id is not null "
                ") "
            "order by ulp_last_enable_time desc, is_enabled desc, position asc, topic_type asc",
            user_id
        );

        result_body = createTopicsList(lang_pairs_result, topics_result, to_translate_lang);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Task<> UserTopics::getTopicGenerationWindow(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto to_translate_lang = req->getHeader("toTranslateLang");

    if (!topic_id_str.size() || to_translate_lang.size() != local_constants::languages::lang_tag_size) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    Json::Value result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);

        auto result = co_await users_db->execSqlCoro(
            "select "
            "topic_id, src_lang, topic_title, order_words_by, "
            "exclude_word_if_not_present_in_all_sources, exclude_word_if_very_rare, "
            "sources_metadata "
            "from user_topics where user_id = $1 and topic_id = $2"
            , user_id, topic_id
        );

        if (!result.size()) {
            throw std::runtime_error("topic_not_exists");
        }

        result_body = createTopicGenerationWindow(result[0], to_translate_lang);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Task<> UserTopics::setTopicTitle(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto topic_title = local_utils::strings::get_title_from_str(req->getHeader("topicTitle"), local_constants::title_max_length);

    if (!topic_id_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);

        auto result = co_await users_db->execSqlCoro("update user_topics set topic_title = $1 where user_id = $2 and topic_id = $3", topic_title, user_id, topic_id);
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

    auto title = local_utils::strings::get_title_from_str(text, local_constants::title_max_length);

    const std::string source_type = "text";

    uint32_t source_id;

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        std::string processed_words = core::text_processing::text_to_processed_source(std::move(unicode_str), topic_src_lang.c_str());

        source_id = co_await core::sql::add_topic_source(users_db, user_id, topic_id, title, source_type, processed_words, text);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);        
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    auto result_body = createTopicGenerationItem(source_id, title, source_type);
    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Task<> UserTopics::getDocumentSupportInfo(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    Json::Value result;
    auto& supported_formats = result["supported_formats"] = Json::arrayValue;
    result["max_size_megabytes"] = 50;

    supported_formats.append("txt");
    supported_formats.append("pdf");
    supported_formats.append("html");

    co_return local_utils::Response::JSON_OK(callback, result);
}

Task<> UserTopics::addTopicSourceDocument(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");

    MultiPartParser multi_part_parser;
    if (multi_part_parser.parse(req) != 0) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "empty");
    }

    auto &files = multi_part_parser.getFiles();

    if (!files.size()) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "no_files");
    }

    auto file = files[0];
    auto content_type = file.getContentType();

    std::string title;
    std::string text;
    std::string fail_type;

    if (content_type == drogon::ContentType::CT_TEXT_PLAIN) {
        text = file.fileContent();
        title = local_utils::strings::get_title_from_str(file.getFileName(), local_constants::title_max_length);
    } else if (content_type == drogon::ContentType::CT_APPLICATION_PDF) {
        text = core::text_extraction::get_text_from_pdf(file.fileData(), file.fileLength());
        title = local_utils::strings::get_title_from_str(file.getFileName(), local_constants::title_max_length);
    } else if (content_type == drogon::ContentType::CT_TEXT_HTML) {
        auto content = std::string(file.fileContent());
        std::tie(title, text) = core::text_extraction::extract_text_and_title_from_html(content);
        title = local_utils::strings::get_title_from_str(title, local_constants::title_max_length);
    } else {
        fail_type = "not_supported_format";
    }

    const std::string source_type = "document";
    uint32_t source_id;

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
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    auto result_body = createTopicGenerationItem(source_id, title, source_type, fail_type);
    co_return local_utils::Response::JSON_OK(callback, result_body);
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
    std::string fail_type;

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

        if (title.empty() || text.empty()) {
            fail_type = "failed_to_fetch_content";
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    const std::string source_type = "website_url";
    uint32_t source_id;

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
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    auto result_body = createTopicGenerationItem(source_id, title, source_type, fail_type);
    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Task<> UserTopics::addTopicSourceYoutubeUrl(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto title = local_utils::strings::get_title_from_str(req->getHeader("title"), local_constants::title_max_length);

    if (!title.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    const std::string source_type = "youtube_url";
    uint32_t source_id;
    std::string fail_type;

    auto text = std::string(req->getBody());

    if (text.empty()) {
        fail_type = "failed_to_fetch_captions";
    }
    
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
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);  
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    auto result_body = createTopicGenerationItem(source_id, title, source_type, fail_type);
    co_return local_utils::Response::JSON_OK(callback, result_body);
}


Task<> UserTopics::deleteTopicSource(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto source_id = req->getHeader("sourceId");

    if (!(topic_id_str.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);

        co_await users_db->execSqlCoro(
            "update user_topics set "
            "sources_metadata = sources_metadata - $1, "
            "sources_unprocessed = sources_unprocessed - $1, "
            "sources_processed = sources_processed - $1, "
            "last_update_id = last_update_id + 1 "
            "where user_id = $2 and topic_id = $3", 
            source_id, user_id, topic_id
        );

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);    
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::getTopicSourceTextContent(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto source_id = req->getHeader("sourceId");

    if (!(topic_id_str.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    std::string result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);

        auto result = co_await users_db->execSqlCoro(
            "select sources_unprocessed->> $1 as body from user_topics where user_id = $2 and topic_id = $3", 
            source_id, user_id, topic_id
        );

        if (!result.size()) {
            throw std::runtime_error("not_found");
        }
        
        result_body = result[0]["body"].as<std::string>();
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, result_body);
}

Task<> UserTopics::setTopicSourceText(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto text = std::string(req->getBody());
    auto source_id = req->getHeader("sourceId");

    if (!(topic_id_str.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    if (!text.size()) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "body_is_empty");
    }

    auto title = local_utils::strings::get_title_from_str(text, local_constants::title_max_length);

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        std::string processed_words = core::text_processing::text_to_processed_source(std::move(unicode_str), topic_src_lang.c_str());

        co_await core::sql::update_topic_source(users_db, user_id, topic_id, std::stoi(source_id), title, "text", processed_words, text);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);    
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::getTopicMixOptions(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
   auto topic_id_str = req->getHeader("topicId");

    if (topic_id_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    Json::Value result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto [topic_id, topic_type] = local_utils::topics::decode_topic_id_str(topic_id_str);

        std::string src_lang;
        std::string dst_lang;
        if (topic_type == local_utils::topics::TopicType::user_topic) {
            auto result = co_await users_db->execSqlCoro("select src_lang, dst_lang from user_topics where user_id = $1 and topic_id = $2",
                user_id, topic_id
            );

            if (result.empty()) {
                throw std::runtime_error("not_exists");
            }

            src_lang = result[0]["src_lang"].as<std::string>();
            dst_lang = result[0]["dst_lang"].as<std::string>();
        } else if (topic_type == local_utils::topics::TopicType::global_topic) {
            auto result = co_await users_db->execSqlCoro("select src_lang, dst_lang from user_global_topics where user_id = $1 and topic_id = $2",
                user_id, static_cast<uint32_t>(topic_id)
            );

            if (result.empty()) {
                throw std::runtime_error("not_exists");
            }

            src_lang = result[0]["src_lang"].as<std::string>();
            dst_lang = result[0]["dst_lang"].as<std::string>();
        }

        co_await core::sql::update_all_user_topics_sequences(users_db, user_id);

        auto result_topics = co_await users_db->execSqlCoro("("
            " select "
                "topic_id, src_lang, dst_lang, mix_value, is_enabled, position, 0 as topic_type,"
                "topic_words_sequence as words_sequence "
                "from user_topics "
                "where user_id = $1 "
                "and topic_id is not null "
                "and is_enabled = true "
                "and src_lang = $2 "
                "and dst_lang = $3 "
                // "and topic_words_sequence != '' "
            " ) "
            " UNION "
            " ( "
            " select "
                "ugt.topic_id, ugt.src_lang, dst_lang, mix_value, is_enabled, position, 1 as topic_type,"
                "gt.words as words_sequence "
                "from user_global_topics as ugt "
                "left join global_topics as gt "
                "on ugt.topic_id = gt.topic_id "
                "and ugt.src_lang = gt.src_lang "
                "where user_id = $1 "
                "and is_enabled = true "
                "and ugt.src_lang = $2 "
                "and dst_lang = $3 "
                "and ugt.topic_id is not null "
                ")"
            " order by is_enabled desc, position asc",
            user_id, src_lang, dst_lang
        );

        result_body["mix_value_multiplier"] = local_constants::topics::mix_value_multiplier;
        result_body["min_mix_value"] = local_constants::topics::min_mix_value;
        result_body["max_mix_value"] = local_constants::topics::max_mix_value;

        std::unordered_set<std::string> words_to_exclude;

        auto& topics_words = result_body["topics_words"] = Json::arrayValue;
        for (auto& topic : result_topics) {
            Json::Value new_topic;
            auto this_topic_id = topic["topic_id"].as<uint64_t>();
            new_topic["this"] = this_topic_id == topic_id;

            std::string words_sequence = topic["words_sequence"].as<std::string>();
            new_topic["words"] = core::text_processing::get_words_sequence_without_duplicates(words_sequence, words_to_exclude);

            new_topic["mix_value"] = topic["mix_value"].as<float>();
            topics_words.append(new_topic);
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Task<> UserTopics::getTopicSkipOptions(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto topic_id_str = req->getHeader("topicId");

    if (topic_id_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    Json::Value result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto [topic_id, topic_type] = local_utils::topics::decode_topic_id_str(topic_id_str);

        if (topic_type == local_utils::topics::TopicType::user_topic) {
            auto result = co_await users_db->execSqlCoro("select skip_value from user_topics where user_id = $1 and topic_id = $2",
                user_id, topic_id
            );

            if (result.empty()) {
                throw std::runtime_error("not_exists");
            }

            result_body["skip_value"] = result[0]["skip_value"].as<float>();
            result_body["words"] = co_await core::sql::get_topic_words_sequence_and_if_update(users_db, user_id, topic_id);
        } else if (topic_type == local_utils::topics::TopicType::global_topic) {
            auto result = co_await users_db->execSqlCoro("select skip_value from user_global_topics where user_id = $1 and topic_id = $2",
                user_id, static_cast<uint32_t>(topic_id)
            );

            if (result.empty()) {
                throw std::runtime_error("not_exists");
            }

            result_body["skip_value"] = result[0]["skip_value"].as<float>();
            result_body["words"] = co_await core::sql::get_global_topic_words_sequence(users_db, topic_id);
        }

        result_body["min_skip_value"] = local_constants::topics::min_skip_value;
        result_body["max_skip_value"] = local_constants::topics::max_skip_value;
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Task<> UserTopics::setTopicOption(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto option_name = req->getHeader("optionName");
    auto value = req->getHeader("value");

    if (!(topic_id_str.size() && option_name.size() && value.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto query = "update user_topics set " + option_name + " = $1, "
            "last_update_id = last_update_id + 1 where user_id = $2 and topic_id = $3";

        static const std::unordered_set<std::string> bolean_options = {"exclude_word_if_not_present_in_all_sources", "exclude_word_if_very_rare"};
        static const std::unordered_set<std::string> int16_options = {"order_words_by"};
        static const std::unordered_set<std::string> float_options = {"mix_value", "skip_value"};

        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);

        if (int16_options.contains(option_name)) {
            auto result = co_await users_db->execSqlCoro(query, static_cast<int16_t>(std::stoi(value)), user_id, topic_id);
            if (!result.affectedRows()) throw std::runtime_error("not_updated");

        } else if (bolean_options.contains(option_name)){
            auto result = co_await users_db->execSqlCoro(query, value == "true" ? true : false, user_id, topic_id);
            if (!result.affectedRows()) throw std::runtime_error("not_updated");

        } else if (float_options.contains(option_name)){
            auto result = co_await users_db->execSqlCoro(query, std::stof(value), user_id, topic_id);
            if (!result.affectedRows()) throw std::runtime_error("not_updated");

        } else {
            co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "unknown_option");
        }

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::setTopicEnabled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto value_str = req->getHeader("value");

    if (!topic_id_str.size() || !value_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);

        auto value = value_str == "true" ? true : false;

        auto result = co_await users_db->execSqlCoro("update user_topics set is_enabled = $1 where user_id = $2 and topic_id = $3", value, user_id, topic_id);
        if (!result.affectedRows()) {
            throw std::runtime_error("not_updated");
        }

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}


Task<> UserTopics::setUserGlobalTopicEnabled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto dst_lang = req->getHeader("dstLang");
    auto value_str = req->getHeader("value");

    if (topic_id_str.empty() || value_str.empty() || dst_lang.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto topic_id = local_utils::topics::decode_global_topic_id_str(topic_id_str);

        auto value = value_str == "true" ? true : false;

        auto result = co_await users_db->execSqlCoro("update user_global_topics set is_enabled = $1 where user_id = $2 and topic_id = $3", value, user_id, topic_id);
        if (!result.affectedRows()) {

            auto src_lang = (co_await users_db->execSqlCoro("select src_lang from global_topics where topic_id = $1 limit 1", topic_id))[0]["src_lang"].as<std::string>();
            auto new_position = co_await core::sql::get_new_biggest_topic_position(users_db, user_id, src_lang, dst_lang);

            result = co_await users_db->execSqlCoro(
                "INSERT INTO user_global_topics (user_id, topic_id, src_lang, dst_lang, dst_title, mix_value, skip_value, position, is_enabled) "
                "SELECT $1, gt.topic_id, gt.src_lang, $2, gt.src_title_en, $3, $4, $5, $6 "
                "FROM global_topics gt WHERE gt.topic_id = $7", 
                user_id, dst_lang, local_constants::topics::mix_value, local_constants::topics::skip_value, new_position, value, topic_id
            );

            if (!result.affectedRows()) {
                throw std::runtime_error("not_updated");
            }
        }

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::setTopicPosition(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto positioin_topic_id_str = req->getHeader("positionTopicId");

    if (!topic_id_str.size() || !positioin_topic_id_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto [position_topic_id, position_topic_type] = local_utils::topics::decode_topic_id_str(positioin_topic_id_str);

        int32_t position;

        if (position_topic_type == local_utils::topics::TopicType::user_topic) {
            auto result = co_await users_db->execSqlCoro("select position from user_topics where user_id = $1 and topic_id = $2", 
                user_id, position_topic_id
            );
            position = result[0]["position"].as<int32_t>();
        } else if (position_topic_type == local_utils::topics::TopicType::global_topic) {
            auto result = co_await users_db->execSqlCoro("select position from user_global_topics where user_id = $1 and topic_id = $2", 
                user_id, static_cast<uint32_t>(position_topic_id)
            );
            position = result[0]["position"].as<int32_t>();
        }
        
        auto [topic_id, topic_type] = local_utils::topics::decode_topic_id_str(topic_id_str);

        if (topic_type == local_utils::topics::TopicType::user_topic) {
            auto result = co_await users_db->execSqlCoro("update user_topics set position = $1 "
                "where user_id = $2 and topic_id = $3 returning src_lang, dst_lang", position, user_id, topic_id
            );
            
            if (!result.affectedRows()) {
                throw std::runtime_error("not_updated");
            }

            auto src_lang = result[0]["src_lang"].as<std::string>();
            auto dst_lang = result[0]["dst_lang"].as<std::string>();

            co_await users_db->execSqlCoro("update user_topics set position = position + 1 "
                "where user_id = $1 and topic_id != $2 and src_lang = $3 and dst_lang = $4 and position >= $5", 
                user_id, topic_id, src_lang, dst_lang, position
            );

            co_await users_db->execSqlCoro("update user_global_topics set position = position + 1 "
                "where user_id = $1 and src_lang = $2 and dst_lang = $3 and position >= $4", 
                user_id, src_lang, dst_lang, position
            );
        } else if (topic_type == local_utils::topics::TopicType::global_topic) {
            auto result = co_await users_db->execSqlCoro("update user_global_topics set position = $1 "
                "where user_id = $2 and topic_id = $3 returning src_lang, dst_lang", 
                position, user_id, static_cast<uint32_t>(topic_id)
            );
            
            if (!result.affectedRows()) {
                throw std::runtime_error("not_updated");
            }

            auto src_lang = result[0]["src_lang"].as<std::string>();
            auto dst_lang = result[0]["dst_lang"].as<std::string>();

            co_await users_db->execSqlCoro("update user_topics set position = position + 1 "
                "where user_id = $1 and src_lang = $2 and dst_lang = $3 and position >= $4", 
                user_id, src_lang, dst_lang, position
            );

            co_await users_db->execSqlCoro("update user_global_topics set position = position + 1 "
                "where user_id = $1 and topic_id != $2 and src_lang = $3 and dst_lang = $4 and position >= $5", 
                user_id, static_cast<uint32_t>(topic_id), src_lang, dst_lang, position
            );
        }

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
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
        auto [topic_id, topic_type] = local_utils::topics::decode_topic_id_str(topic_id_str);

        if (topic_type == local_utils::topics::TopicType::user_topic) {
            result_body = co_await core::sql::get_topic_words_sequence_and_if_update(users_db, user_id, topic_id);
        } else if (topic_type == local_utils::topics::TopicType::global_topic) {
            result_body = co_await core::sql::get_global_topic_words_sequence(users_db, topic_id);
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, result_body);
}

Json::Value UserTopics::createTopicGenerationItem(uint32_t id, const std::string& title, const std::string& type, const std::string& fail_type)
{
    Json::Value result;
    result["id"] = id;
    result["title"] = title;
    result["type"] = type;
    if (fail_type.size()) {
        result["fail_type"] = fail_type;
    }
    return result;
}

Json::Value UserTopics::createTopicGenerationWindow(const drogon::orm::Result::Reference& row, const std::string& to_translate_lang)
{
    Json::Value result;
    result["topic_id"] = local_utils::topics::encode_topic_id(
        row["topic_id"].as<uint64_t>(), 
        local_utils::topics::TopicType::user_topic
    );
    auto src_lang = row["src_lang"].as<std::string>();
    result["src_lang"] = src_lang;
    result["src_lang_name"] = local_utils::supported_languages::get_language_title_localized(src_lang, to_translate_lang);
    result["topic_title"] = row["topic_title"].as<std::string>();

    auto& options = result["options"];
    options["order_words_by"] = row["order_words_by"].as<uint32_t>();
    options["exclude_word_if_not_present_in_all_sources"] = row["exclude_word_if_not_present_in_all_sources"].as<bool>();
    options["exclude_word_if_very_rare"] = row["exclude_word_if_very_rare"].as<bool>();

    auto& sources = result["sources"] = Json::arrayValue;

    auto sources_metadata_str = row["sources_metadata"].as<std::string>();

    Json::Value sources_metadata_root;
    if (!Json::Reader().parse(sources_metadata_str, sources_metadata_root)) {
        throw std::runtime_error("metadata_parse_error");
    }

    for (const auto& id : sources_metadata_root.getMemberNames()) {
        auto& ref = sources_metadata_root[id];
        ref["id"] = std::stoi(id);
        sources.append(ref);
    }

    return result;
}

Json::Value UserTopics::createTopicsList(const drogon::orm::Result& lang_pairs_rows, const drogon::orm::Result& topics_rows, const std::string& to_translate_lang)
{
    Json::Value result;
    auto& topics = result["topics"] = Json::arrayValue;
    auto& language_pairs = result["language_pairs"] = Json::arrayValue;

    uint32_t last_lang_pair_id = 0;
    std::unordered_map<std::string, uint32_t> language_pairs_ids;

    for (auto& row : lang_pairs_rows) {
        auto src_lang = row["src_lang"].as<std::string>();
        auto dst_lang = row["dst_lang"].as<std::string>();
        auto lang_pair_key = src_lang + dst_lang;

        language_pairs_ids[lang_pair_key] = last_lang_pair_id++;

        Json::Value new_lang_pair;
        new_lang_pair["src_lang"] = src_lang;
        new_lang_pair["dst_lang"] = dst_lang;
        new_lang_pair["lang_mix_value"] = row["mix_value"].as<float>();

        new_lang_pair["src_lang_name"] = local_utils::supported_languages::get_language_title_localized(src_lang, to_translate_lang);
        new_lang_pair["dst_lang_name"] = local_utils::supported_languages::get_language_title_localized(dst_lang, to_translate_lang);
        language_pairs.append(new_lang_pair);
    }

    for (auto& row : topics_rows) {
        auto src_lang = row["src_lang"].as<std::string>();
        auto dst_lang = row["dst_lang"].as<std::string>();
        
        auto lang_pair_key = src_lang + dst_lang;
        auto current_lang_pair_id = language_pairs_ids[lang_pair_key];

        Json::Value new_topic;

        new_topic["topic_title"] = row["topic_title"].as<std::string>();
        new_topic["mix_value"] = row["mix_value"].as<float>();
        new_topic["skip_value"] = row["skip_value"].as<float>();
        new_topic["is_enabled"] = row["is_enabled"].as<bool>();
        new_topic["position"] = row["position"].as<int32_t>();
        new_topic["lang_pair_index"] = current_lang_pair_id;

        auto topic_type = static_cast<local_utils::topics::TopicType>(row["topic_type"].as<uint32_t>());

        new_topic["topic_id"] = local_utils::topics::encode_topic_id(row["topic_id"].as<uint64_t>(), topic_type);

        new_topic["type"] = local_utils::topics::topic_type_to_str(topic_type);

        if (topic_type == local_utils::topics::TopicType::user_topic) {
            new_topic["order_words_by"] = row["order_words_by"].as<int32_t>();
        }

        topics.append(new_topic);
    }

    return result;
}
}
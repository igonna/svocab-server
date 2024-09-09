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

        auto result_position = co_await users_db->execSqlCoro("select COALESCE(MIN(position), 0) AS biggest_position "
            "from user_topics where user_id = $1", user_id
        );

        auto position = result_position[0]["biggest_position"].as<int32_t>() - 1;

        auto result = co_await users_db->execSqlCoro("insert into user_topics (user_id, src_lang, dst_lang, position) "
            "values ($1, $2, $3, $4) returning "
            "topic_id, src_lang, topic_title, count_word_as, order_words_by, "
            "exclude_word_if_not_present_in_all_sources, exclude_word_if_very_rare, "
            "sources_metadata, sources_unprocessed, sources_processed"
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
    Json::Value result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro(
            "select ut.topic_id, ut.src_lang, ut.dst_lang, ulp.mix_value as lang_mix_value, ut.topic_title, ut.mix_value, ut.skip_value, ut.is_enabled, ut.position "
            "from user_topics as ut "
            "join user_language_pairs as ulp "
            "on ut.user_id = ulp.user_id "
            "and ut.src_lang = ulp.src_lang "
            "and ut.dst_lang = ulp.dst_lang "
            "where ulp.is_enabled = true "
            "and ulp.user_id = $1 "
            "order by ulp.last_enable_time desc, ut.position asc",
            user_id
        );

        result_body = createTopicsList(result);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_body);
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
            "sources_metadata "
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

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoull(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        std::string processed_words = core::text_processing::text_to_processed_words(std::move(unicode_str), topic_src_lang.c_str());

        co_await core::sql::add_topic_source(users_db, user_id, topic_id, title, "text", processed_words, text);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);        
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
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

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoull(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        std::string processed_words = core::text_processing::text_to_processed_words(std::move(unicode_str), topic_src_lang.c_str());

        co_await core::sql::add_topic_source(users_db, user_id, topic_id, title, "document", processed_words);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
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

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoull(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        std::string processed_words = core::text_processing::text_to_processed_words(std::move(unicode_str), topic_src_lang.c_str());

        co_await core::sql::add_topic_source(users_db, user_id, topic_id, title, "website_url", processed_words);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
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

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoull(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        std::string processed_words = core::text_processing::text_to_processed_words(std::move(unicode_str), topic_src_lang.c_str());

        co_await core::sql::add_topic_source(users_db, user_id, topic_id, title, "youtube_url", processed_words);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);  
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
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
        auto topic_id = std::stoull(topic_id_str);

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
        auto topic_id = std::stoull(topic_id_str);

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

    auto title = text.substr(0, local_constants::title_max_length);

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoull(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        std::string processed_words = core::text_processing::text_to_processed_words(std::move(unicode_str), topic_src_lang.c_str());

        co_await core::sql::update_topic_source(users_db, user_id, topic_id, std::stoi(source_id), title, "text", processed_words, text);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);    
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
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
        static const std::unordered_set<std::string> int16_options = {"count_word_as", "order_words_by"};

        if (int16_options.contains(option_name)) {
            auto result = co_await users_db->execSqlCoro(query, static_cast<int16_t>(std::stoi(value)), user_id, std::stoull(topic_id_str));
            if (!result.affectedRows()) throw std::runtime_error("not_updated");

        } else if (bolean_options.contains(option_name)){
            auto result = co_await users_db->execSqlCoro(query, value == "t" ? true : false, user_id, std::stoull(topic_id_str));
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
    auto value = req->getHeader("value");

    if (!topic_id_str.size() || !value.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("update user_topics set is_enabled = $1 where user_id = $2 and topic_id = $3", value, user_id, std::stoull(topic_id_str));
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

Task<> UserTopics::setTopicPosition(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto position_str = req->getHeader("position");

    if (!topic_id_str.size() || !position_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto position = std::stoi(position_str);
        auto topic_id = std::stoull(topic_id_str);

        auto result = co_await users_db->execSqlCoro("update user_topics set position = $1 where user_id = $2 and topic_id = $3", position, user_id, topic_id);
        if (!result.affectedRows()) {
            throw std::runtime_error("not_updated");
        }

        result = co_await users_db->execSqlCoro("update user_topics set position = position + 1 where user_id = $1 and topic_id != $2 and position >= $3", user_id, topic_id, position);
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

Json::Value UserTopics::createTopicsList(const drogon::orm::Result& rows)
{
    Json::Value result;
    auto& topics = result["topics"] = Json::arrayValue;
    auto& language_pairs = result["language_pairs"] = Json::arrayValue;

    uint32_t last_lang_pair_id = 0;
    std::unordered_map<std::string, uint32_t> language_pairs_ids;

    for (auto& row : rows) {
        auto src_lang = row["src_lang"].as<std::string>();
        auto dst_lang = row["dst_lang"].as<std::string>();
        
        auto key = src_lang + dst_lang;
        uint32_t current_lang_pair_id;

        if (language_pairs_ids.contains(key)) {
            current_lang_pair_id = language_pairs_ids[key];
        } else {
            language_pairs_ids[key] = current_lang_pair_id = last_lang_pair_id++;

            Json::Value new_lang_pair;
            new_lang_pair["src_lang"] = src_lang;
            new_lang_pair["dst_lang"] = dst_lang;
            new_lang_pair["lang_mix_value"] = row["lang_mix_value"].as<float>();
            language_pairs.append(new_lang_pair);
        }

        Json::Value new_topic;
        new_topic["topic_id"] = row["topic_id"].as<uint64_t>();
        new_topic["topic_title"] = row["topic_title"].as<std::string>();
        new_topic["mix_value"] = row["mix_value"].as<float>();
        new_topic["skip_value"] = row["skip_value"].as<float>();
        new_topic["is_enabled"] = row["is_enabled"].as<bool>();
        new_topic["position"] = row["position"].as<int32_t>();
        new_topic["lang_pair_index"] = current_lang_pair_id;

        topics.append(new_topic);
    }

    return result;
}
}
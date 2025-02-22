#include "Topics.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/text_extraction/html_extractor.h"
#include "core/text_extraction/pdf_extractor.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"
#include "core/text_processing/text_processor.h"
#include "core/topic_extractor/topic_extractor_factory.h"

namespace v1
{
Task<> Topics::createTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto src_lang = req->getHeader("srcLang");
    auto dst_lang = req->getHeader("dstLang");

    if (!src_lang.size() || !dst_lang.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        core::topic_extractor::TopicExtractorFactory topic_extractor_factory;
        auto topic_extractor = co_await topic_extractor_factory.create_extractor(req);

        auto title = topic_extractor->get_title();
        auto origin = topic_extractor->get_origin();
        auto origin_type = topic_extractor->get_origin_type();
        auto words_sequence = topic_extractor->get_words_sequence();

        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto current_time = local_utils::time::get_unix_timestamp();

        auto result = co_await users_db->execSqlCoro(
            "insert into user_topics (user_id, src_lang, dst_lang, created_at, title, origin, origin_type, words_sequence)",
            " values ($1, $2, $3, $4, $5, $6, $7, $8)",
            user_id, src_lang, dst_lang, current_time, title, origin, origin_type, words_sequence
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("topic_not_created");
        }
    } catch (const std::exception& e) {
        // !body->isMember("origin") || body->isMember("type") || body->isMember("content")
        co_return local_utils::Response::Error::NOT_VALID_BODY(callback);
    } catch (const std::exception& e) {
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> Topics::deleteTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
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

Task<> Topics::getTopics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
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
            " where user_id = $1 and is_enabled = true order by last_enable_time asc", user_id
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
            "order by ulp_last_enable_time asc, is_enabled desc, position asc, topic_type asc",
            user_id
        );

        result_body = createTopicsList(lang_pairs_result, topics_result, to_translate_lang);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Task<> Topics::setTopicTitle(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
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

Task<bool> update_user_topic_title_if_empty(drogon::orm::DbClientPtr users_db, std::string topic_title, uint64_t user_id, uint64_t topic_id) {
    auto result = co_await users_db->execSqlCoro("update user_topics set topic_title = $1 where user_id = $2 and topic_id = $3 and LENGTH(topic_title) = 0", topic_title, user_id, topic_id);
    co_return result.affectedRows();
}

Task<> Topics::addTopicSourceText(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
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
    bool topic_title_updated = false;

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = local_utils::topics::decode_user_topic_id_str(topic_id_str);

        auto topic_src_lang = co_await core::sql::get_user_topic_src_lang(users_db, user_id, topic_id);

        auto unicode_str = core::text_processing::string_to_unicode(text);
        std::string processed_words = core::text_processing::text_to_processed_source(std::move(unicode_str), topic_src_lang.c_str());

        source_id = co_await core::sql::add_topic_source(users_db, user_id, topic_id, title, source_type, processed_words, text);
        topic_title_updated = topic_title_updated = co_await update_user_topic_title_if_empty(users_db, title, user_id, topic_id);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);        
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    auto result_body = createTopicGenerationItem(source_id, title, source_type, "", topic_title_updated);
    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Task<> Topics::getSupportedDocumentTypes(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    Json::Value result;
    auto& supported_formats = result["supported_formats"] = Json::arrayValue;
    result["max_size_megabytes"] = 50;

    supported_formats.append("txt");
    supported_formats.append("pdf");
    supported_formats.append("html");

    co_return local_utils::Response::JSON_OK(callback, result);
}

Task<> Topics::addTopicSourceDocument(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
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

Task<> Topics::addTopicSourceYoutubeUrl(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
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

Task<> Topics::getTopicSourceTextContent(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
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

Task<> Topics::setTopicSourceText(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto topic_id_str = req->getHeader("topicId");
    auto text = std::string(req->getBody());
    auto source_id_str = req->getHeader("sourceId");

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

        source_id = std::stoi(source_id_str);
        co_await core::sql::update_topic_source(users_db, user_id, topic_id, source_id, title, source_type, processed_words, text);
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);    
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    auto result_body = createTopicGenerationItem(source_id, title, source_type);
    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Task<> Topics::getTopicWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
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

Json::Value Topics::createTopicsList(const drogon::orm::Result& lang_pairs_rows, const drogon::orm::Result& topics_rows, const std::string& to_translate_lang)
{
    Json::Value result;
    auto& language_pairs = result["language_pairs"] = Json::arrayValue;

    std::unordered_map<std::string, Json::Value*> lang_pair_topics;

    for (auto& row : lang_pairs_rows) {
        auto src_lang = row["src_lang"].as<std::string>();
        auto dst_lang = row["dst_lang"].as<std::string>();

        Json::Value new_lang_pair;
        new_lang_pair["src_lang"] = src_lang;
        new_lang_pair["dst_lang"] = dst_lang;
        new_lang_pair["lang_mix_value"] = row["mix_value"].as<float>();

        new_lang_pair["src_lang_name"] = local_utils::supported_languages::get_language_title_localized(src_lang, to_translate_lang);
        new_lang_pair["dst_lang_name"] = local_utils::supported_languages::get_language_title_localized(dst_lang, to_translate_lang);

        auto& ref_to_topics = new_lang_pair["topics"] = Json::arrayValue;
        auto lang_pair_key = src_lang + dst_lang;
        lang_pair_topics[lang_pair_key] = &ref_to_topics;

        language_pairs.append(std::move(new_lang_pair));
    }

    for (auto& row : topics_rows) {
        auto src_lang = row["src_lang"].as<std::string>();
        auto dst_lang = row["dst_lang"].as<std::string>();
        
        auto lang_pair_key = src_lang + dst_lang;

        if (lang_pair_topics.contains(lang_pair_key)) {
            auto& lang_pair_topics_ref = lang_pair_topics[lang_pair_key];
            lang_pair_topics_ref->append(createTopicJson(row));
        }
    }

    return result;
}
}
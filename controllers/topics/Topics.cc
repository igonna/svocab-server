#include "Topics.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/text_extraction/html_extractor.h"
#include "core/text_extraction/pdf_extractor.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"
#include "core/text_processing/text_processor.h"
#include "core/topic_extractor/topic_extractor_factory.h"
#include "utils/exceptions.h"
#include "plugins/FrequentWords/FrequentWords.h"

namespace v1
{
Task<> Topics::createTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto type = req->getHeader("type");

    if (type.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto latest_lang_pair = co_await core::sql::language_pair::get_latest_enabled_language_pair(users_db, user_id);

        if (!latest_lang_pair.has_value()) {
            co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "language_pair_not_exists");
        }

        auto &latest_lang_pair_value = latest_lang_pair.value();
        const auto& src_lang = latest_lang_pair_value.first;
        const auto& dst_lang = latest_lang_pair_value.second;

        core::topic_extractor::TopicExtractorFactory topic_extractor_factory;
        auto topic_extractor = co_await topic_extractor_factory.create_extractor(req, src_lang, dst_lang);

        auto title = topic_extractor->get_title();
        auto origin = topic_extractor->get_origin();
        auto origin_type = topic_extractor->get_origin_type();
        auto words_sequence = topic_extractor->get_words_sequence();

        auto current_time = local_utils::time::get_unix_timestamp();

        auto result = co_await users_db->execSqlCoro(
            "insert into user_topics (user_id, src_lang, dst_lang, created_at, title, origin, origin_type, words_sequence, last_update_time)"
            " values ($1, $2, $3, $4, $5, $6, $7, $8, $9)",
            user_id, src_lang, dst_lang, current_time, title, origin, origin_type, words_sequence, current_time
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("topic_not_created");
        }

        co_await core::sql::update_user_group_last_update_id(users_db, user_id);
        co_return local_utils::Response::OK(callback, "");
    } catch (const exception::unknown_type& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::not_valid_body& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::not_valid_header& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::data_extract_fail& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::systems_are_busy& e) { co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, e.what());
    } catch (const exception::file_processing_limit_reached& e) { co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, e.what());
    } catch (const exception::prompt_topic_generation_limit_reached& e) { co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, e.what());
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Topics::appendContent(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto type = req->getHeader("type");
    auto topic_id_str = req->getHeader("topicId");

    if (type.empty() || topic_id_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto topic_id = std::stoul(topic_id_str);

        // get existing topic content
        auto old_topic_result = co_await users_db->execSqlCoro(
            "select src_lang, dst_lang, words_sequence from user_topics where user_id = $1 and topic_id = $2", user_id, topic_id
        );

        if (old_topic_result.empty()) {
            co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "topic_not_exists");
        }

        auto src_lang = old_topic_result[0]["src_lang"].as<std::string>();
        auto dst_lang = old_topic_result[0]["dst_lang"].as<std::string>();

        if (old_topic_result.empty()) {
            co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "languages_do_not_match");
        }

        // extract new content
        core::topic_extractor::TopicExtractorFactory topic_extractor_factory;
        auto topic_extractor = co_await topic_extractor_factory.create_extractor(req, src_lang, dst_lang);
        auto words_sequence = topic_extractor->get_words_sequence();

        // create updated content
        words_sequence += " " + old_topic_result[0]["words_sequence"].as<std::string>();
        req->addHeader("type", "text");
        req->setBody(std::move(words_sequence));

        auto updated_topic_extractor = co_await topic_extractor_factory.create_extractor(req, src_lang, dst_lang);
        auto updated_words_sequence = updated_topic_extractor->get_words_sequence();

        auto current_time = local_utils::time::get_unix_timestamp();
        auto result = co_await users_db->execSqlCoro(
            "update user_topics set last_update_time = $1, words_sequence = $2 where user_id = $3 and topic_id = $4",
            current_time, updated_words_sequence, user_id, topic_id
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("topic_not_updated");
        }

        co_await core::sql::update_user_group_last_update_id(users_db, user_id);
        co_return local_utils::Response::OK(callback, "");
    } catch (const exception::unknown_type& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::not_valid_body& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::not_valid_header& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::data_extract_fail& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::systems_are_busy& e) { co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, e.what());
    } catch (const exception::file_processing_limit_reached& e) { co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, e.what());
    } catch (const exception::prompt_topic_generation_limit_reached& e) { co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, e.what());
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Topics::getTopicsTitles(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto topics_result = co_await users_db->execSqlCoro(
            " WITH max_lang_pair AS ("
                " SELECT src_lang, dst_lang"
                " FROM user_language_pairs"
                " WHERE user_id = $1 AND is_enabled = true"
                " ORDER BY last_enable_time DESC"
                " LIMIT 1"
            " )"
            " SELECT"
                " ut.topic_id,"
                " ut.title"
            " FROM user_topics AS ut"
            " JOIN max_lang_pair AS mlp"
                " ON ut.src_lang = mlp.src_lang"
                " AND ut.dst_lang = mlp.dst_lang"
            " WHERE ut.user_id = $1"
                " AND ut.is_enabled = true"
                " AND ut.topic_id IS NOT NULL"
            " ORDER BY ut.created_at DESC", user_id
        );

        auto json_names = createTopicsTitlesList(topics_result);
        co_return local_utils::Response::JSON_OK(callback, json_names);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Topics::getTopics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto to_translate_lang = req->getHeader("toTranslateLang");

    if (to_translate_lang.size() != local_constants::languages::lang_tag_size) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto lang_pairs_result = co_await users_db->execSqlCoro(
            " SELECT src_lang, dst_lang"
            " FROM user_language_pairs"
            " WHERE user_id = $1 AND is_enabled = true"
            " ORDER BY last_enable_time DESC", user_id
        );

        auto topics_result = co_await users_db->execSqlCoro(
            " select"
                " ut.topic_id,"
                " ulp.src_lang,"
                " ulp.dst_lang,"
                " ut.title,"
                " ut.created_at,"
                " ut.is_enabled,"
                " ulp.last_enable_time as ulp_last_enable_time"
            " from user_language_pairs as ulp"
            " left join user_topics as ut"
            " on ut.user_id = ulp.user_id"
            " and ut.src_lang = ulp.src_lang"
            " and ut.dst_lang = ulp.dst_lang"
            " where ulp.is_enabled = true and ut.is_enabled = true"
            " and ulp.user_id = $1"
            " and topic_id is not null"
            " order by ulp_last_enable_time asc, created_at desc", user_id
        );

        auto json_topics = createTopicsList(lang_pairs_result, topics_result, to_translate_lang);
        co_return local_utils::Response::JSON_OK(callback, json_topics);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Topics::getTopicProgress(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto topic_id_str = req->getHeader("topicId");

    if (topic_id_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoul(topic_id_str);

        auto topic_result = co_await users_db->execSqlCoro(
            "select src_lang, dst_lang, words_sequence from user_topics where user_id = $1 and topic_id = $2",
            user_id, topic_id
        );

        if (topic_result.empty()) {
            co_return local_utils::Response::Error::ERROR(callback, k404NotFound, "topic_not_exists");
        }

        const auto& src_lang = topic_result[0]["src_lang"].as<std::string>();
        const auto& dst_lang = topic_result[0]["dst_lang"].as<std::string>();
        const auto& words_sequence = topic_result[0]["words_sequence"].as<std::string>();

        auto frequent_words_plugin = app().getPlugin<FrequentWords>();
        auto words_sequence_vec = core::text_processing::extract_words_from_str(words_sequence, src_lang.c_str());
        auto user_words_progress = co_await core::sql::user_words::get_user_words_progress(users_db, user_id, src_lang.c_str(), words_sequence_vec);

        auto topic_words_progress_list = createTopicProgressList(words_sequence_vec, user_words_progress);
        co_return local_utils::Response::OK(callback, topic_words_progress_list);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Topics::renameTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto new_title = std::string(req->getBody());
    auto topic_id_str = req->getHeader("topicId");

    if (topic_id_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    if (new_title.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_BODY(callback);
    }

    try {
        auto topic_id = std::stoul(topic_id_str);
        new_title = local_utils::strings::get_title_from_str(new_title, local_constants::title_max_length);

        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto result = co_await users_db->execSqlCoro("update user_topics set title = $1 where user_id = $2 and topic_id = $3",
            new_title, user_id, topic_id
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("topic_not_updated");
        }

        co_return local_utils::Response::OK(callback, "");
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Topics::updateTopicContent(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto topic_id_str = req->getHeader("topicId");

    if (topic_id_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    if (!req->bodyLength()) {
        co_return local_utils::Response::Error::NOT_VALID_BODY(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto topic_id = std::stoul(topic_id_str);

        // get existing topic info
        auto old_topic_result = co_await users_db->execSqlCoro(
            "select src_lang, dst_lang from user_topics where user_id = $1 and topic_id = $2", user_id, topic_id
        );

        if (old_topic_result.empty()) {
            co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "topic_not_exists");
        }

        auto src_lang = old_topic_result[0]["src_lang"].as<std::string>();
        auto dst_lang = old_topic_result[0]["dst_lang"].as<std::string>();

        if (old_topic_result.empty()) {
            co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "languages_do_not_match");
        }

        req->addHeader("type", "text");
        core::topic_extractor::TopicExtractorFactory topic_extractor_factory;
        auto topic_extractor = co_await topic_extractor_factory.create_extractor(req, src_lang, dst_lang);
        auto words_sequence = topic_extractor->get_words_sequence();

        auto current_time = local_utils::time::get_unix_timestamp();
        auto result = co_await users_db->execSqlCoro(
            "update user_topics set last_update_time = $1, words_sequence = $2 where user_id = $3 and topic_id = $4",
            current_time, words_sequence, user_id, topic_id
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("topic_not_updated");
        }

        co_await core::sql::update_user_group_last_update_id(users_db, user_id);
        co_return local_utils::Response::OK(callback, "");
    } catch (const exception::unknown_type& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::not_valid_body& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::not_valid_header& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::data_extract_fail& e) { co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, e.what());
    } catch (const exception::systems_are_busy& e) { co_return local_utils::Response::Error::ERROR(callback, k503ServiceUnavailable, e.what());
    } catch (const exception::file_processing_limit_reached& e) { co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, e.what());
    } catch (const exception::prompt_topic_generation_limit_reached& e) { co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, e.what());
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Topics::deleteTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto topic_id_str = req->getHeader("topicId");

    if (topic_id_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto topic_id = std::stoul(topic_id_str);

        co_await users_db->execSqlCoro("delete from user_topics where user_id = $1 and topic_id = $2", user_id, topic_id);
        co_await core::sql::update_user_group_last_update_id(users_db, user_id);
        co_return local_utils::Response::OK(callback, "");
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
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

Json::Value Topics::createTopicsList(const drogon::orm::Result& lang_pairs_rows, const drogon::orm::Result& topics_rows, const std::string& to_translate_lang) {
    Json::Value result;
    auto& language_pairs = result["languages"] = Json::arrayValue;

    std::unordered_map<std::string, Json::Value*> lang_pair_topics;

    for (auto& row : lang_pairs_rows) {
        auto src_lang = row["src_lang"].as<std::string>();
        auto dst_lang = row["dst_lang"].as<std::string>();

        Json::Value new_lang_pair;
        new_lang_pair["src_lang"] = src_lang;
        new_lang_pair["dst_lang"] = dst_lang;
        new_lang_pair["src_lang_name"] = local_utils::supported_languages::get_language_title_localized(src_lang, to_translate_lang);

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

            Json::Value new_topic;
            new_topic["topic_id"] = row["topic_id"].as<std::string>();
            new_topic["title"] = row["title"].as<std::string>();
            lang_pair_topics_ref->append(std::move(new_topic));
        }
    }

    return result;
}

Json::Value Topics::createTopicsTitlesList(const drogon::orm::Result& topics_rows) {
    Json::Value result;

    auto& topics_titles = result["topics_ids_titles"] = Json::arrayValue;

    for (auto& row : topics_rows) {
        Json::Value new_id_title;
        new_id_title["topic_id"] = row["topic_id"].as<std::string>();
        new_id_title["title"] = row["title"].as<std::string>();
        topics_titles.append(std::move(new_id_title));
    }

    return result;
}

std::string Topics::createTopicProgressList(const std::vector<std::string>& topic_words, const std::unordered_map<std::string, std::pair<bool, uint16_t>> words_progress) {
    std::string result;
    uint8_t status = 0;

    for (const auto& word : topic_words) {
        auto word_progress = words_progress.find(word);
        if (word_progress != words_progress.end()) {
            status = word_progress->second.first 
                ? 2 
                : (word_progress->second.second >= local_constants::learn_progress::mastered_confirmation_level ? 1 : 0);
        } else {
            status = 0;
        }
        result += word + ';' + std::to_string(status) + '\n';
    }

    if (result.size()) result.pop_back();

    return result;
}
}
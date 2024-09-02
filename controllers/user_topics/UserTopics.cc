#include "UserTopics.h"
#include "utils/utils.h"
#include "utils/constants.h"

namespace v1
{
Task<> UserTopics::enableFirstUserTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Json::Value createTopicGenerationWindow(const drogon::orm::Result::Reference& row) {
    Json::Value result;
    result["topic_id"] = row["topic_id"].as<uint32_t>();

    result["src_lang"] = row["src_lang"].as<std::string>();

    result["topic_title"] = row["topic_title"].as<std::string>();

    auto& options = result["options"];
    options["word_count_type"] = row["word_count_type"].as<uint32_t>();
    options["word_order_type"] = row["word_order_type"].as<uint32_t>();
    options["exclude_words_that_not_in_all_sources"] = row["exclude_words_that_not_in_all_sources"].as<bool>();
    options["exclude_very_rare_words"] = row["exclude_very_rare_words"].as<bool>();

    auto& sources = result["sources"] = Json::arrayValue;

    static auto new_source = [](std::string* title, std::string from, std::string type) {
        Json::Value source;
        source["title"] = *title;
        source["from"] = from; // text, document, website, youtube
        source["type"] = type; // source, fail
        return source;
    };

    // text
    auto texts_titles = row["texts_titles"].asArray<std::string>();
    for (auto& i : texts_titles) {
        sources.append(new_source(i.get(), "text", "source"));
    }

    auto texts_fail_title_status = row["texts_fail_title_status"].asArray<std::string>();
    for (auto& i : texts_fail_title_status) {
        sources.append(new_source(i.get(), "text", "fail"));
    }
    
    // document
    auto documents_titles = row["documents_titles"].asArray<std::string>();
    for (auto& i : documents_titles) {
        sources.append(new_source(i.get(), "document", "source"));
    }

    auto documents_fail_title_status = row["documents_fail_title_status"].asArray<std::string>();
    for (auto& i : documents_fail_title_status) {
        sources.append(new_source(i.get(), "document", "fail"));
    }
    
    // website
    auto website_urls_titles = row["website_urls_titles"].asArray<std::string>();
    for (auto& i : website_urls_titles) {
        sources.append(new_source(i.get(), "website_url", "source"));
    }

    auto website_urls_fail_title_status = row["website_urls_fail_title_status"].asArray<std::string>();
    for (auto& i : website_urls_fail_title_status) {
        sources.append(new_source(i.get(), "website_url", "fail"));
    }
    
    auto youtube_urls_titles = row["youtube_urls_titles"].asArray<std::string>();
    for (auto& i : youtube_urls_titles) {
        sources.append(new_source(i.get(), "youtube_url", "source"));
    }
    
    auto youtube_urls_fail_title_status = row["youtube_urls_fail_title_status"].asArray<std::string>();
    for (auto& i : youtube_urls_fail_title_status) {
        sources.append(new_source(i.get(), "youtube_url", "fail"));
    }

    return result;
}

Task<> updateUserSequenceLastUpdateId(const drogon::orm::DbClientPtr& users_db, uint32_t user_id) {
    auto result = co_await users_db->execSqlCoro("update user_sequence set last_update_id = last_update_id + 1 where user_id = $1", user_id);
    if (!result.affectedRows()) {
        throw std::runtime_error("not_updated");
    }
    co_return;
}

Task<> UserTopics::createTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto src_lang = req->getHeader("srcLang");
    auto dst_lang = req->getHeader("dstLang");

    if (!(src_lang.size() && dst_lang.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    Json::Value resultTopic;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::user_id);
        uint32_t position = 0;

        auto result = co_await users_db->execSqlCoro("insert into user_topics (user_id, src_lang, dst_lang, position) "
            "values ($1, $2, $3, $4) returning "
            "topic_id, src_lang, topic_title, word_count_type, word_order_type, "
            "exclude_words_that_not_in_all_sources, exclude_very_rare_words, "
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

Task<> UserTopics::deleteTopic(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto topic_id_str = req->getHeader("topicId");

    if (!topic_id_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::user_id);

        co_await users_db->execSqlCoro("delete from user_topics where user_id = $1 and topic_id = $2", user_id, std::stoi(topic_id_str));

        co_await updateUserSequenceLastUpdateId(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::getUserTopics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserTopics::getTopicGenerationWindow(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto topic_id = req->getHeader("topicId");

    if (!topic_id.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    Json::Value result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro(
            "select "
            "topic_id, src_lang, topic_title, word_count_type, word_order_type, "
            "exclude_words_that_not_in_all_sources, exclude_very_rare_words, "
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

Task<> UserTopics::setTopicTitle(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto topic_id_str = req->getHeader("topicId");
    auto topic_title = req->getHeader("topicTitle");

    if (!topic_id_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("update user_topics set topic_title = $1 where user_id = $2 and topic_id = $3", topic_title, user_id, std::stoi(topic_id_str));
        if (!result.affectedRows()) {
            throw std::runtime_error("not_updated");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::addTopicSource(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserTopics::deleteTopicSource(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserTopics::deleteTopicFailedSource(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserTopics::getTopicSourceTextContent(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserTopics::setTopicSourceText(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserTopics::addTopicSourceDocument(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserTopics::setTopicOption(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserTopics::setTopicEnabled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto topic_id_str = req->getHeader("topicId");
    auto value = req->getHeader("value");

    if (!topic_id_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("update user_topics set is_enabled = $1 where user_id = $2 and topic_id = $3", value, user_id, std::stoi(topic_id_str));
        if (!result.affectedRows()) {
            throw std::runtime_error("not_updated");
        }

        // update sequence last_update_id
        co_await updateUserSequenceLastUpdateId(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserTopics::setTopicPosition(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    co_return;
}

Task<> UserTopics::getTopicWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto topic_id_str = req->getHeader("topicId");

    if (!topic_id_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    std::string result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("select topic_processed_words from user_topics where user_id = $1 and topic_id = $2", user_id, std::stoi(topic_id_str));
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
}
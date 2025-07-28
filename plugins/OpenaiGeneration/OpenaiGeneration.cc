#include "OpenaiGeneration.h"
#include "utils/utils.h"
#include "core/text_processing/text_processor.h"
#include <drogon/drogon.h>
#include "utils/constants.h"
#include "core/postgres/queries.h"
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <unicode/uloc.h>
#include <ranges>

using namespace drogon;

OpenaiGeneration::OpenaiGeneration() {}

void OpenaiGeneration::initAndStart(const Json::Value &config) {
    api_key = config["api_key"].asString();
    model = config["model"].asString();
    url = config["url"].asString();

    auto rpm_limit = config["rpm_limit"].asUInt();
    free_users_openai_rpm_limit = rpm_limit - rpm_limit * 0.2f;
}

void OpenaiGeneration::shutdown() {}

drogon::Task<Json::Value> OpenaiGeneration::openai_get_response(const std::string& prompt) {
    auto [host, path] = local_utils::url::extract_host_and_path_from_url(url);

    auto client = HttpClient::newHttpClient(host);

    Json::Value request_body;
    request_body["model"] = model;
    auto& messages = request_body["messages"] = Json::arrayValue;

    Json::Value message;
    message["role"] = "user";
    message["content"] = prompt;

    messages.append(message);

    auto request = HttpRequest::newHttpJsonRequest(request_body);
    request->addHeader("Content-Type", "application/json");
    request->addHeader("Authorization", api_key);

    request->setPath(path);
    request->setMethod(Post);

    auto response = co_await client->sendRequestCoro(request);

    if (response->getStatusCode() != k200OK) {
        throw response_code_is_not_200_exception();
    } else {
        auto response_body_str = std::string(response->getBody());

        Json::Value response_json;
        if (!Json::Reader().parse(response_body_str, response_json)) {
            LOG_ERROR << "openai response parse error";
            throw json_parse_failed_exception();
        }

        co_return response_json;
    }
}

drogon::Task<bool> OpenaiGeneration::openai_is_word_translation_valid(const std::string& src_lang_name, const std::string& dst_lang_name, const std::string& src_word, const std::string& dst_word) {
    std::string prompt = std::string("is ") + dst_lang_name + " \'" + dst_word + "\' a possible translation for " + src_lang_name + " \'" + src_word + "\', output only yes or no";

    auto response = co_await openai_get_response(prompt);
    auto response_message = response["choices"][0]["message"]["content"].as<std::string>();

    auto unicode_str = core::text_processing::string_to_unicode(response_message);
    auto words = core::text_processing::get_words_data_from_text(std::move(unicode_str), "en");

    co_return words.contains("yes");
}

std::string OpenaiGeneration::correct_phrase(std::string&& phrase, const std::string& lang) {
    auto unicode_phrase = core::text_processing::string_to_unicode(phrase);
    unicode_phrase.trim();

    if (unicode_phrase.endsWith(".")) {
        unicode_phrase.truncate(unicode_phrase.length() - 1);
    }

    unicode_phrase.trim();

    const UChar double_quote_char = u'"';
    int32_t pos = 0;
    while ((pos = unicode_phrase.indexOf(double_quote_char, pos)) != -1) {
        unicode_phrase.remove(pos, 1);
    }

    if (unicode_phrase.length()) {
        UChar upper_char = u_toupper(unicode_phrase.charAt(0));
        unicode_phrase.setCharAt(0, upper_char);
    }

    core::text_processing::replace_apostrophe_for_lang(unicode_phrase, lang);

    std::string utf8_phrase;
    unicode_phrase.toUTF8String(utf8_phrase);

    return utf8_phrase;
}

std::string OpenaiGeneration::correct_word(std::string&& word, const std::string& lang) {
    auto unicode_word = core::text_processing::string_to_unicode(word);
    unicode_word.toLower();

    unicode_word.trim();

    const UChar double_quote_char = u'"';
    int32_t pos = 0;
    while ((pos = unicode_word.indexOf(double_quote_char, pos)) != -1) {
        unicode_word.remove(pos, 1);
    }

    core::text_processing::replace_apostrophe_for_lang(unicode_word, lang);

    std::string utf8_word;
    unicode_word.toUTF8String(utf8_word);

    return utf8_word;
}

bool is_valid_dst_word(const icu::UnicodeString& word) {
    UChar32 whitespace = ' ';

    for (int32_t i = 0; i < word.length(); ++i) {
        UChar32 c = word.char32At(i);

        if (!(u_isalpha(c) || whitespace)) {
            return false;
        }
    }

    return true;
}

std::tuple<std::string, std::string, std::string> OpenaiGeneration::extract_word_phrase_from_openai_response(
    const std::string& response, const std::string& src_word, const std::string& src_lang, const std::string& dst_lang
) {
    std::stringstream response_stream(response);
    std::string part;
    std::vector<std::string> parts;

    while(std::getline(response_stream, part, ';')) {
        parts.push_back(part);
    }

    if (parts.size() != 3) {
        throw not_valid_response_format_exception();
    }

    auto src_phrase = correct_phrase(std::move(parts[0]), src_lang);
    auto dst_phrase = correct_phrase(std::move(parts[1]), dst_lang);
    auto dst_word = correct_word(std::move(parts[2]), dst_lang);

    if (src_phrase.empty() || dst_phrase.empty() || dst_word.empty()) {
        throw not_valid_response_format_exception();
    }

    auto src_phrase_words = core::text_processing::get_words_data_from_text(
        core::text_processing::string_to_unicode(src_phrase),
        src_lang.c_str()
    );

    if (src_phrase_words.size() == 1) {
        throw not_valid_response_format_exception();
    }

    auto src_phrase_unicode = core::text_processing::string_to_unicode(src_phrase).toLower();
    auto src_word_unicode = core::text_processing::string_to_unicode(src_word).toLower();

    if (src_phrase_unicode.indexOf(src_word_unicode) == -1) {
        throw phrase_not_contains_a_word_exception();
    }

    // if (!src_phrase_words.contains(src_word)) {
    //     throw phrase_not_contains_a_word_exception();
    // }

    auto unicode_dst_phrase = core::text_processing::string_to_unicode(dst_phrase);
    auto dst_phrase_words = core::text_processing::get_words_data_from_text(std::move(unicode_dst_phrase), dst_lang.c_str());

    if (dst_phrase_words.size() == 1) {
        throw not_valid_response_format_exception();
    }

    auto unicode_dst_word = core::text_processing::string_to_unicode(dst_word);

    if(!is_valid_dst_word(unicode_dst_word)) {
        throw dst_word_not_valid_exception();
    }

    auto dst_word_parts = core::text_processing::get_words_data_from_text(std::move(unicode_dst_word), dst_lang.c_str());

    // validate that word not contains non alpha or non whitespace character

    auto dst_phrase_unicode = core::text_processing::string_to_unicode(dst_phrase).toLower();
    auto dst_word_unicode = core::text_processing::string_to_unicode(dst_word).toLower();

    if (dst_phrase_unicode.indexOf(dst_word_unicode) == -1) {
        throw phrase_not_contains_a_word_exception();
    }

    // for (auto& dst_word_part : dst_word_parts) {
    //     if (!dst_phrase_words.contains(dst_word_part.first)) {
    //         throw phrase_not_contains_a_word_exception();
    //     }
    // }

    return {src_phrase, dst_phrase, dst_word};
}

drogon::Task<std::optional<std::tuple<uint32_t, std::string, std::string, std::string>>> OpenaiGeneration::get_word_phrase_translation(drogon::orm::DbClientPtr translations_db, const std::string& src_lang, const std::string& dst_lang, const std::string& src_word) {
    auto word_phrase_table_name = local_utils::translations_db::create_word_phrase_table_name(src_lang);

    auto word_translation_result = co_await translations_db->execSqlCoro(
        "select id, word_translation, phrase, phrase_translation from " + word_phrase_table_name + " where dst_lang = $1 and word = $2 limit 1",
        dst_lang, src_word
    );

    if (word_translation_result.size()) {
        const auto& row_ref = word_translation_result.at(0);

        auto id = row_ref["id"].as<uint32_t>();
        auto src_phrase = row_ref["phrase"].as<std::string>();
        auto dst_phrase = row_ref["phrase_translation"].as<std::string>();
        auto dst_word = row_ref["word_translation"].as<std::string>();

        co_return std::make_optional(std::make_tuple(id, src_phrase, dst_phrase, dst_word));
    }

    auto banned_word_table_name = local_utils::translations_db::create_banned_word_table_name(src_lang);

    auto banned_word_result = co_await translations_db->execSqlCoro(
        "select 1 from " + banned_word_table_name + " where word = $1 limit 1", src_word
    );

    if (banned_word_result.size()) {
        throw word_not_valid_exception();
    }

    co_return std::nullopt;
}

// return true if limit ok
drogon::Task<bool> OpenaiGeneration::increase_free_users_openai_limit() {
    try {
        auto rc = app().getRedisClient();

        static const std::string script = "if redis.call('EXISTS', KEYS[1]) == 0 then"
                " redis.call('INCRBY', KEYS[1], ARGV[1])"
                " redis.call('EXPIRE', KEYS[1], ARGV[2])"
            " else"
                " redis.call('INCRBY', KEYS[1], ARGV[1])"
            " end"
            " return redis.call('GET', KEYS[1])";

        int increment = 1;

        auto result = co_await rc->execCommandCoro("EVAL %s 1 %s %d %d",
            script.c_str(),
            local_constants::redis::openai_free_users_limit_key_name.c_str(),
            increment,
            local_constants::openai_free_users_limit_key_expire_seconds
        );

        if (result.isNil()) {
            co_return true;
        }

        auto cur_limit = std::stoi(result.asString());

        co_return cur_limit < free_users_openai_rpm_limit;
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return true;
    }
}

drogon::Task<> OpenaiGeneration::check_openai_limit(bool has_subscription) {
    if (!has_subscription) {
        auto limit_ok = co_await increase_free_users_openai_limit();
        if (!limit_ok) {
            throw systems_are_busy_exception();
        }
    }
}

std::string create_word_generation_key(const std::string& src_lang, const std::string& dst_lang, const std::string& src_word) {
    return src_lang + ';' + dst_lang + ';' + src_word;
}

drogon::Task<> OpenaiGeneration::await_other_server_word_generation(const std::string& src_lang, const std::string& dst_lang, const std::string& src_word) {
    auto rc = app().getRedisClient();

    const auto word_key = create_word_generation_key(src_lang, dst_lang, src_word);

    uint32_t seconds_passed = 0;

    while(seconds_passed++ < word_generation_expire_seconds) {
        auto result = co_await rc->execCommandCoro("exists %s", word_key);

        if (result.isNil()) {
            co_return;
        }

        if (!std::stoi(result.asString())) {
            co_return;
        }

        co_await sleepCoro(app().getLoop(), std::chrono::seconds{1});
    }
}

drogon::Task<> OpenaiGeneration::set_generating_word(const std::string& word_key) {
    auto rc = app().getRedisClient();
    co_await rc->execCommandCoro("set %s %s ex %d nx", word_key.c_str(), "", word_generation_expire_seconds);
}

drogon::Task<> OpenaiGeneration::delete_generating_word(const std::string& word_key) {
    auto rc = app().getRedisClient();
    co_await rc->execCommandCoro("del %s", word_key);
}

drogon::Task<GeneratedPhrase> OpenaiGeneration::generate_phrase(const std::string& src_lang, const std::string& dst_lang, const std::string& src_word, bool has_subscription) {
    co_await await_other_server_word_generation(src_lang, dst_lang, src_word);

    auto translations_db = app().getDbClient(local_constants::translations_db);

    auto is_word_banned = co_await core::sql::is_banned_word_exists(translations_db, src_lang, src_word);

    if (is_word_banned) {
        throw word_not_valid_exception();
    }

    auto word_phrase_translation_opt = co_await get_word_phrase_translation(translations_db, src_lang, dst_lang, src_word);

    if (word_phrase_translation_opt.has_value()) {
        auto value = word_phrase_translation_opt.value();
        auto [id, src_phrase, dst_phrase, dst_word] = *word_phrase_translation_opt;

        src_phrase = correct_phrase(std::move(src_phrase), src_lang);
        dst_phrase = correct_phrase(std::move(dst_phrase), dst_lang);
        dst_word = correct_word(std::move(dst_word), dst_lang);

        co_return GeneratedPhrase(id, src_lang, dst_lang, src_word, std::move(dst_word), std::move(src_phrase), std::move(dst_phrase));
    }

    const auto src_lang_name = local_utils::supported_languages::get_language_title_localized(src_lang, "en");
    const auto dst_lang_name = local_utils::supported_languages::get_language_title_localized(dst_lang, "en");

    const std::string prompt = std::string("generate ") + src_lang_name + " meaningful phrase from word '" 
        + src_word + "' and translate this phrase to " + dst_lang_name + " and extract word translation from phrase,"
        + " output this only as csv \"" + src_lang_name + " phrase\" ; \"" + dst_lang_name + " phrase\" ; \"extracted '" + src_word + "' word translation from " + dst_lang_name + " phrase\"";

    const auto word_key = create_word_generation_key(src_lang, dst_lang, src_word);

    uint32_t retry_count = 0;
    const uint32_t max_retries = 3;

    bool add_banned_word = false;

    try {
        while(retry_count < max_retries) {

            try {
                co_await check_openai_limit(has_subscription);

                co_await set_generating_word(word_key);

                auto response = co_await openai_get_response(prompt);
                auto response_message = response["choices"][0]["message"]["content"].as<std::string>();

                auto [src_phrase, dst_phrase, dst_word] = extract_word_phrase_from_openai_response(response_message, src_word, src_lang, dst_lang);

                co_await check_openai_limit(has_subscription);
                auto is_word_translation_valid = co_await openai_is_word_translation_valid(src_lang_name, dst_lang_name, src_word, dst_word);

                if (is_word_translation_valid) {
                    auto id = co_await core::sql::add_word_phrase_translation(translations_db, src_lang, dst_lang, src_word, dst_word, src_phrase, dst_phrase);
                    co_await delete_generating_word(word_key);
                    co_return GeneratedPhrase(id, src_lang, dst_lang, src_word, std::move(dst_word), std::move(src_phrase), std::move(dst_phrase));
                }
            } catch (const not_valid_response_format_exception& e) {
                // retry
            } catch (const json_parse_failed_exception& e) {
                // retry
            } catch (const phrase_not_contains_a_word_exception& e) {
                // retry
            } catch (const dst_word_not_valid_exception& e) {
                // retry
            }

            ++retry_count;
        }

        add_banned_word = true;

    } catch (const systems_are_busy_exception& e) {
        throw;
    } catch (const response_code_is_not_200_exception& e) {
        throw systems_are_busy_exception();
    } catch (const std::exception& e) {
        throw word_not_valid_exception();
    }

    if (add_banned_word) {
        co_await core::sql::add_banned_word(translations_db, src_lang, src_word);
        co_await delete_generating_word(word_key);
        throw word_not_valid_exception();
    }

    throw word_not_valid_exception();
}

drogon::Task<std::string> OpenaiGeneration::generate_words_list(const std::string& lang, const std::string& topic, bool has_subscription) {
    const auto lang_name = local_utils::supported_languages::get_language_title_localized(lang, "en");

    const std::string prompt = std::string("generate up to 60 popular ") + lang_name + " words for this topic '" + topic + "', output only words separated by ,";

    uint32_t retry_count = 0;
    const uint32_t max_retries = 3;

    try {
        while(retry_count < max_retries) {

            try {
                co_await check_openai_limit(has_subscription);

                auto response = co_await openai_get_response(prompt);
                auto response_message = response["choices"][0]["message"]["content"].as<std::string>();

                co_return response_message;
            } catch (const json_parse_failed_exception& e) {
                // retry
            } catch (const phrase_not_contains_a_word_exception& e) {
                // retry
            }

            ++retry_count;
        }

    } catch (const std::exception& e) {
        throw systems_are_busy_exception();
    }

    co_return "";
}
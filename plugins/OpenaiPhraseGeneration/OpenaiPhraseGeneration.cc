#include "OpenaiPhraseGeneration.h"
#include "utils/utils.h"
#include "core/text_processing/text_processor.h"
#include <unicode/uchar.h>

using namespace drogon;

OpenaiPhraseGeneration::OpenaiPhraseGeneration() {}

void OpenaiPhraseGeneration::initAndStart(const Json::Value &config) {
    api_key = config["api_key"].asString();
    model = config["model"].asString();
    url = config["url"].asString();
}

void OpenaiPhraseGeneration::shutdown() {}

drogon::Task<Json::Value> OpenaiPhraseGeneration::openai_get_response(const std::string& prompt) {
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

drogon::Task<bool> OpenaiPhraseGeneration::openai_is_word_translation_valid(const std::string& src_lang_name, const std::string& dst_lang_name, const std::string& src_word, const std::string& dst_word) {
    std::string prompt = std::string("is ") + dst_lang_name + " \'" + dst_word + "\' valid translation for " + src_lang_name + " \'" + src_word + "\', output only yes or no";

    auto response = co_await openai_get_response(prompt);
    auto response_message = response["choices"][0]["message"]["content"].as<std::string>();

    auto unicode_str = core::text_processing::string_to_unicode(response_message);
    auto words = core::text_processing::get_words_data_from_text(std::move(unicode_str), "en");

    co_return words.contains("yes");
}

std::string OpenaiPhraseGeneration::correct_phrase(std::string&& phrase) {
    auto unicode_phrase = core::text_processing::string_to_unicode(phrase);
    unicode_phrase.trim();

    if (unicode_phrase.endsWith(".")) {
        unicode_phrase.truncate(unicode_phrase.length() - 1);
    }

    unicode_phrase.trim();

    if (unicode_phrase.length()) {
        UChar upper_char = u_toupper(unicode_phrase.charAt(0));
        unicode_phrase.setCharAt(0, upper_char);
    }

    std::string utf8_phrase;
    unicode_phrase.toUTF8String(utf8_phrase);

    return utf8_phrase;
}

std::string OpenaiPhraseGeneration::correct_word(std::string&& word) {
    auto unicode_word = core::text_processing::string_to_unicode(word);
    unicode_word.toLower();

    unicode_word.trim();

    std::string utf8_word;
    unicode_word.toUTF8String(utf8_word);

    return utf8_word;
}

std::tuple<std::string, std::string, std::string> OpenaiPhraseGeneration::extract_word_phrase_from_openai_response(const std::string& response, const std::string& src_word, const std::string& src_lang, const std::string& dst_lang) {
    std::string src_phrase, dst_phrase, dst_word;

    auto str_size = response.size();

    bool started = false;
    uint8_t part_num = 0;

    for (uint32_t i = 0; i < str_size; ++i) {
        if (response[i] == '"') {
            started = !started;
            if (started) {
                ++part_num;
            }
            continue;
        }

        if (started) {
            if (part_num == 1) {
                src_phrase += response[i];
            } else if (part_num == 2) {
                dst_phrase += response[i];
            } else if (part_num == 3) {
                dst_word += response[i];
            } else {
                throw not_valid_response_format_exception();
            }
        }
    }

    src_phrase = correct_phrase(std::move(src_phrase));
    dst_phrase = correct_phrase(std::move(dst_phrase));
    dst_word = correct_word(std::move(dst_word));

    if (src_phrase.empty() || dst_phrase.empty() || dst_word.empty()) {
        throw not_valid_response_format_exception();
    }

    auto unicode_src_phrase = core::text_processing::string_to_unicode(src_phrase);
    auto src_phrase_words = core::text_processing::get_words_data_from_text(std::move(unicode_src_phrase), src_lang.c_str());

    if (src_phrase_words.size() == 1) {
        throw not_valid_response_format_exception();
    }

    if (!src_phrase_words.contains(src_word)) {
        throw phrase_not_contains_a_word_exception();
    }

    auto unicode_dst_phrase = core::text_processing::string_to_unicode(dst_phrase);
    auto dst_phrase_words = core::text_processing::get_words_data_from_text(std::move(unicode_dst_phrase), dst_lang.c_str());

    if (dst_phrase_words.size() == 1) {
        throw not_valid_response_format_exception();
    }

    auto unicode_dst_word = core::text_processing::string_to_unicode(dst_word);
    auto dst_word_parts = core::text_processing::get_words_data_from_text(std::move(unicode_dst_word), dst_lang.c_str());

    for (auto& dst_word_part : dst_word_parts) {
        if (!dst_phrase_words.contains(dst_word_part.first)) {
            throw phrase_not_contains_a_word_exception();
        }
    }

    return {src_phrase, dst_phrase, dst_word};
}

// phrase not contains a word means word is not valid

// check if word has translation [db]
// check if word not in black list [db]
// check if user premium allowed to generate [redis]
// check if word already generating by another server [redis]
// on generation completed save word in database [db]
// on generation completed save word in black listed [db]

drogon::Task<std::tuple<std::string, std::string, std::string>> OpenaiPhraseGeneration::generate_phrase(const std::string& src_lang, const std::string& dst_lang, const std::string& src_word, bool has_subscription) {
    std::string src_phrase, dst_phrase, dst_word;

    auto src_lang_name = local_utils::supported_languages::get_language_title_localized(src_lang, "en");
    auto dst_lang_name = local_utils::supported_languages::get_language_title_localized(dst_lang, "en");

    std::string prompt = std::string("generate ") + src_lang_name + " meaningful phrase from word '" 
        + src_word + "' and translate this phrase to " + dst_lang_name + " and extract word translation from phrase,"
        + " output this only as csv \"" + src_lang_name + " phrase\" ; \"" + dst_lang_name + " phrase\" ; \"extracted '" + src_word + "' word translation from " + dst_lang_name + " phrase\"";

    try {
        auto response = co_await openai_get_response(prompt);
        auto response_message = response["choices"][0]["message"]["content"].as<std::string>();

        // std::cout << std::endl << std::endl << "src_word: " << src_word << std::endl << "src_lang: " << src_lang << std::endl;
        // std::cout << "response: " << response_message << std::endl;

        auto [src_phrase, dst_phrase, dst_word] = extract_word_phrase_from_openai_response(response_message, src_word, src_lang, dst_lang);
        auto is_word_translation_valid = co_await openai_is_word_translation_valid(src_lang_name, dst_lang_name, src_word, dst_word); // not valid word

        // std::cout << "processed: " << src_phrase << " ; " << dst_phrase << " ; " << dst_word << std::endl;
        // std::cout << "valid translation: " << (is_word_translation_valid ? "true" : "false") << std::endl;

    } catch (const systems_are_busy_exception& e) {
        throw;
    } catch (const not_valid_response_format_exception& e) {
        // retry
        std::cout << "exception: " << e.what() << std::endl;
    } catch (const json_parse_failed_exception& e) {
        // retry
        std::cout << "exception: " << e.what() << std::endl;
    } catch (const response_code_is_not_200_exception& e) {
        throw systems_are_busy_exception();
    } catch (const phrase_not_contains_a_word_exception& e) {
        throw word_not_valid_exception();
    }

    co_return {src_phrase, dst_phrase, dst_word};
}
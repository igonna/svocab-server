#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>

struct WordPhrasePair {
	WordPhrasePair(WordPhrasePair&& other) : src_phrase(std::move(other.src_phrase)), dst_phrase(std::move(other.dst_phrase)), dst_word(std::move(other.dst_word)) {}
	WordPhrasePair(std::string&& src_phrase, std::string&& dst_phrase, std::string&& dst_word) 
		: src_phrase(std::move(src_phrase)), dst_phrase(std::move(dst_phrase)), dst_word(std::move(dst_word)) {}

	std::string src_phrase;
	std::string dst_phrase;
	std::string dst_word;
};

class OpenaiPhraseGeneration : public drogon::Plugin<OpenaiPhraseGeneration> {
public:
	OpenaiPhraseGeneration();
	void initAndStart(const Json::Value &config) override;
	void shutdown() override;

	drogon::Task<std::tuple<std::string, std::string, std::string>> generate_phrase(const std::string& src_lang, const std::string& dst_lang, const std::string& src_word, bool has_subscription);

public:
	// exceptions
	struct systems_are_busy_exception : public std::exception {
		const char* what() const noexcept override {
			return "systems_are_busy";
		}
	};

	struct word_not_valid_exception : public std::exception {
		const char* what() const noexcept override {
			return "word_not_valid";
		}
	};

private:
	// private exceptions
	struct json_parse_failed_exception : public std::exception {
		const char* what() const noexcept override {
			return "json_parse_failed";
		}
	};

	struct not_valid_response_format_exception : public std::exception {
		const char* what() const noexcept override {
			return "not_valid_response_format";
		}
	};

	struct phrase_not_contains_a_word_exception : public std::exception {
		const char* what() const noexcept override {
			return "phrase_not_contains_a_word";
		}
	};

	struct response_code_is_not_200_exception : public std::exception {
		const char* what() const noexcept override {
			return "response_code_is_not_200";
		}
	};

private:
	drogon::Task<Json::Value> openai_get_response(const std::string& prompt);
	drogon::Task<bool> openai_is_word_translation_valid(const std::string& src_lang_name, const std::string& dst_lang_name, const std::string& src_word, const std::string& dst_word);
	std::tuple<std::string, std::string, std::string> extract_word_phrase_from_openai_response(const std::string& response, const std::string& src_word, const std::string& src_lang, const std::string& dst_lang);
	std::string correct_phrase(std::string&& phrase);
	std::string correct_word(std::string&& word);

private:
	std::string api_key;
	std::string model;
	std::string url;
};
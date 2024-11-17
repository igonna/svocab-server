#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>
#include <drogon/orm/DbClient.h>

struct GeneratedPhrase {
	GeneratedPhrase(GeneratedPhrase&& other) 
		: src_lang(std::move(src_lang)), src_word(std::move(other.src_word)), dst_word(std::move(other.dst_word)), src_phrase(std::move(other.src_phrase)), dst_phrase(std::move(other.dst_phrase)) {}
	GeneratedPhrase(std::string&& src_lang, std::string&& src_word, std::string&& dst_word, std::string&& src_phrase, std::string&& dst_phrase) 
		: src_lang(std::move(src_lang)), src_word(std::move(src_word)), dst_word(std::move(dst_word)), src_phrase(std::move(src_phrase)), dst_phrase(std::move(dst_phrase)) {}
	GeneratedPhrase(std::string src_lang, std::string src_word, std::string&& dst_word, std::string&& src_phrase, std::string&& dst_phrase) 
		: src_lang(src_lang), src_word(src_word), dst_word(std::move(dst_word)), src_phrase(std::move(src_phrase)), dst_phrase(std::move(dst_phrase)) {}
	
	std::string src_lang;
	std::string src_word;
	std::string dst_word;
	std::string src_phrase;
	std::string dst_phrase;
};

class OpenaiPhraseGeneration : public drogon::Plugin<OpenaiPhraseGeneration> {
public:
	OpenaiPhraseGeneration();
	void initAndStart(const Json::Value &config) override;
	void shutdown() override;
	
	drogon::Task<GeneratedPhrase> generate_phrase(const std::string& src_lang, const std::string& dst_lang, const std::string& src_word, bool has_subscription);

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

	struct dst_word_not_valid_exception : public std::exception {
		const char* what() const noexcept override {
			return "dst_word_not_valid";
		}
	};

private:
	drogon::Task<Json::Value> openai_get_response(const std::string& prompt);
	drogon::Task<bool> openai_is_word_translation_valid(const std::string& src_lang_name, const std::string& dst_lang_name, const std::string& src_word, const std::string& dst_word);
	std::tuple<std::string, std::string, std::string> extract_word_phrase_from_openai_response(const std::string& response, const std::string& src_word, const std::string& src_lang, const std::string& dst_lang);
	std::string correct_phrase(std::string&& phrase);
	std::string correct_word(std::string&& word);

	drogon::Task<std::optional<std::tuple<std::string, std::string, std::string>>> get_word_phrase_translation(drogon::orm::DbClientPtr translations_db, const std::string& src_lang, const std::string& dst_lang, const std::string& src_word);

	drogon::Task<bool> increase_free_users_openai_limit();

	drogon::Task<> await_word_generation(const std::string& src_lang, const std::string& dst_lang, const std::string& src_word);
	drogon::Task<> set_generating_word(const std::string& word_key);
	drogon::Task<> delete_generating_word(const std::string& word_key);

private:
	std::string api_key;
	std::string model;
	std::string url;
	uint32_t free_users_openai_rpm_limit;
	uint32_t word_generation_expire_seconds = 6;
};
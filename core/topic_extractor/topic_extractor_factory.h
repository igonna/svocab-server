#pragma once

#include <map>
#include <memory>
#include <drogon/utils/coroutine.h>
#include <drogon/HttpController.h>
#include "utils/exceptions.h"

namespace core::topic_extractor {


struct TopicExctractor {
	virtual ~TopicExctractor() = default;
	virtual drogon::Task<> init(drogon::HttpRequestPtr req) = 0;

	inline std::string get_title() const { return title; }
	inline std::string get_origin() const { return origin; }
	inline std::string get_origin_type() const { return origin_type; }
	inline std::string get_words_sequence() const { return words_sequence; }

protected:
	std::string title;
	std::string origin;
	std::string origin_type;
	std::string words_sequence;
};


struct PromptExtractor : public TopicExctractor {
	drogon::Task<> init(drogon::HttpRequestPtr req) override;
};

struct LinkExtractor : public TopicExctractor {
	drogon::Task<> init(drogon::HttpRequestPtr req) override;
};

struct TextExtractor : public TopicExctractor {
	drogon::Task<> init(drogon::HttpRequestPtr req) override;
};

struct YoutubeExtractor : public TopicExctractor {
	drogon::Task<> init(drogon::HttpRequestPtr req) override;
};

struct HtmlExtractor : public TopicExctractor {
	drogon::Task<> init(drogon::HttpRequestPtr req) override;
};

struct FileExtractor : public TopicExctractor {
	drogon::Task<> init(drogon::HttpRequestPtr req) override;
};


struct TopicExtractorFactory {

	static drogon::Task<std::unique_ptr<TopicExctractor>> create_extractor(drogon::HttpRequestPtr req) {
		auto type = req->getHeader("type");

		if (!req->bodyLength()) throw exception::not_valid_body();

		std::unique_ptr<TopicExctractor> topic_extractor;

		if (type == "prompt") {
			topic_extractor = std::make_unique<PromptExtractor>();
		} else if ("link") {
			topic_extractor = std::make_unique<LinkExtractor>();
		} else if ("text") {
			topic_extractor = std::make_unique<TextExtractor>();
		} else if ("youtube") {
			topic_extractor = std::make_unique<YoutubeExtractor>();
		} else if ("html") {
			topic_extractor = std::make_unique<HtmlExtractor>();
		} else if ("file") {
			topic_extractor = std::make_unique<FileExtractor>();
		} else {
			throw exception::unknown_type();
		}

		co_await topic_extractor->init(req);
		co_return topic_extractor;
	}
};

};
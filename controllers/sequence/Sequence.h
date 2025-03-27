#pragma once

#include <drogon/HttpController.h>
#include <core/user_sequence_words/user_sequence_words.h>
#include <plugins/OpenaiGeneration/OpenaiGeneration.h>
#include "core/phrases_generation/phrases_generator.h"

using namespace drogon;

namespace v1
{
class Sequence : public drogon::HttpController<Sequence> {
public:
	Task<> getItem(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(Sequence::getItem, "/get_item", Post, "JwtFilter");
	METHOD_LIST_END
};
}
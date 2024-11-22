#pragma once

#include <drogon/HttpController.h>
#include <core/user_sequence_words/user_sequence_words.h>
#include <plugins/OpenaiPhraseGeneration/OpenaiPhraseGeneration.h>

using namespace drogon;

namespace v1
{
class UserFocusGroup : public drogon::HttpController<UserFocusGroup>
{
public:
	Task<> getFocusGroupLastUpdateTime(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getPreviousFocusGroupResult(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getFocusGroupWithHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> setWordPhrasePassed(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> setSpecialOfferPassed(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> completeFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserFocusGroup::getFocusGroupLastUpdateTime, "/get_focus_group_last_update_time", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::getFocusGroup, "/get_focus_group", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::getPreviousFocusGroupResult, "/get_previous_focus_group_result", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::getFocusGroupWithHiddenWord, "/get_focus_group_with_hidden_word", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::setWordPhrasePassed, "/set_word_phrase_passed", Post, "JwtFilter");
	METHOD_ADD(UserFocusGroup::setSpecialOfferPassed, "/set_special_offer_passed", Post, "JwtFilter");
	METHOD_ADD(UserFocusGroup::completeFocusGroup, "/complete_focus_group", Post, "JwtFilter");	
	METHOD_LIST_END

private:
	Task<> increase_generated_phrases_count(drogon::orm::DbClientPtr users_db, uint64_t user_id);
	Task<std::tuple<bool, uint32_t>> is_user_generation_limit_reached(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool user_has_subscription);

	Task<std::vector<core::user_sequence_words::SequenceUserWord>> get_updated_user_words_sequence(
		drogon::orm::DbClientPtr users_db,
		uint64_t user_id,
		uint32_t sequence_last_update_id
	);

	Task<std::optional<std::string>> get_user_special_offer_type(drogon::orm::DbClientPtr users_db, uint64_t user_id);

	Task<uint32_t> update_focus_group_last_update_time(drogon::orm::DbClientPtr users_db, uint64_t user_id);

	Json::Value create_focus_group_json(const std::vector<GeneratedPhrase>& focus_group_phrases, uint32_t focus_group_id);
};
}
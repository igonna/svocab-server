#pragma once

#include <drogon/HttpController.h>
#include <core/user_sequence_words/user_sequence_words.h>
#include <plugins/OpenaiPhraseGeneration/OpenaiPhraseGeneration.h>
#include "core/phrases_generation/phrases_generator.h"

using namespace drogon;

namespace focus_group_generation_exceptions {
	struct systems_are_busy_exception : public std::exception {
		const char* what() const noexcept override {
			return "systems_are_busy";
		}
	};

	struct user_blocked_from_generation : public std::exception {
		const char* what() const noexcept override {
			return "user_blocked_from_generation";
		}
	};
}

namespace v1
{
class UserFocusGroup : public drogon::HttpController<UserFocusGroup>
{
public:
	Task<> isFocusGroupUpdateRequired(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getPreviousFocusGroupResult(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getFocusGroupWithHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> setCurrentWordPhase(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> setSpecialOfferPassed(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> completeFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserFocusGroup::isFocusGroupUpdateRequired, "/is_focus_group_update_required", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::getFocusGroup, "/get_focus_group", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::getPreviousFocusGroupResult, "/get_previous_focus_group_result", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::getFocusGroupWithHiddenWord, "/get_focus_group_with_hidden_word", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::setCurrentWordPhase, "/set_current_word_phase", Post, "JwtFilter");
	METHOD_ADD(UserFocusGroup::setSpecialOfferPassed, "/set_special_offer_passed", Post, "JwtFilter");
	METHOD_ADD(UserFocusGroup::completeFocusGroup, "/complete_focus_group", Post, "JwtFilter");	
	METHOD_LIST_END

private:
	Task<> increase_generated_phrases_count(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint16_t count);
	Task<std::tuple<bool, uint32_t>> is_user_generation_limit_reached(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool user_has_subscription);

	Task<std::vector<core::user_sequence_words::SequenceUserWord>> get_updated_user_words_sequence(
		drogon::orm::DbClientPtr users_db,
		uint64_t user_id,
		uint32_t sequence_last_update_id
	);

	Task<std::optional<std::string>> get_user_special_offer_type(drogon::orm::DbClientPtr users_db, uint64_t user_id);

	Task<uint32_t> update_focus_group_last_update_time(drogon::orm::DbClientPtr users_db, uint64_t user_id);

	Json::Value create_focus_group_json(
		uint32_t current_time,
		const std::vector<core::phrases_generator::FocusGroupPhrase>& focus_group_phrases, 
		uint32_t focus_group_id,
		uint32_t learned_this_time,
		bool is_multiple_languages,
		uint32_t device_num,
		const std::string& special_offer_type = ""
	);

	void clear_focus_group_from_technical_data(Json::Value& focus_group);
	std::tuple<uint32_t, uint32_t> decode_word_id(const std::string& word_id_str);

	Json::Value get_focus_group_summary(const Json::Value& focus_group);

	Task<bool> is_user_blocked_from_generation(uint64_t user_id);
	Task<> block_user_from_generation(uint64_t user_id);

	Task<uint32_t> get_new_focus_group_id(drogon::orm::DbClientPtr users_db, uint64_t user_id);

	Task<std::vector<core::phrases_generator::FocusGroupPhrase>> generate_focus_group_phrases(
		drogon::orm::DbClientPtr users_db,
		uint32_t user_id,
		bool user_has_subscription,
		uint32_t sequence_last_update_id,
		std::vector<core::user_sequence_words::SequenceUserWord>& user_words_sequence,
		uint32_t focus_group_size
	);

	Task<uint32_t> get_learned_this_time(drogon::orm::DbClientPtr users_db, uint64_t user_id);
	Task<bool> get_is_multiple_languages(drogon::orm::DbClientPtr users_db, uint64_t user_id);
};
}
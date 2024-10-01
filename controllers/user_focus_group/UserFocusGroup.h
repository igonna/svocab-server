#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserFocusGroup : public drogon::HttpController<UserFocusGroup>
{
public:
	Task<> prepareFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getPreviousFocusGroupResult(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> getFocusGroupWithHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> updateWordPhase(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
	Task<> completeFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserFocusGroup::prepareFocusGroup, "/prepare_focus_group", Post, "JwtFilter");
	METHOD_ADD(UserFocusGroup::getFocusGroup, "/get_focus_group", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::getPreviousFocusGroupResult, "/get_previous_focus_group_result", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::getFocusGroupWithHiddenWord, "/get_focus_group_with_hidden_word", Get, "JwtFilter");
	METHOD_ADD(UserFocusGroup::updateWordPhase, "/update_word_phase", Post, "JwtFilter");
	METHOD_ADD(UserFocusGroup::completeFocusGroup, "/complete_focus_group", Post, "JwtFilter");	
	METHOD_LIST_END
};
}
#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserRegister : public drogon::HttpController<UserRegister>
{
  public:
    Task<> registerWithoutEmail(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
    Task<> getCodeForEmailRegister(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
    Task<> registerEmail(HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback);
    Task<> registerGoogle(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

    // list path definitions here;
    // PATH_ADD("/path", "filter1", "filter2", HttpMethod1, HttpMethod2...);

    // METHOD_ADD(User::getInfo, "/{id}", Get);

    METHOD_LIST_BEGIN
    METHOD_ADD(UserRegister::registerWithoutEmail, "/without_email", Post);
    METHOD_ADD(UserRegister::getCodeForEmailRegister, "/get_code_for_email", Post);
    METHOD_ADD(UserRegister::registerEmail, "/email", Post);
    METHOD_ADD(UserRegister::registerGoogle, "/google", Post);
    METHOD_LIST_END
};
}
#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserRegister : public drogon::HttpController<UserRegister>
{
  public:
    void registerWithoutEmail(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const;
    void getCodeForEmailRegister(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const;
    void registerEmail(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const;
    void registerGoogle(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const;

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
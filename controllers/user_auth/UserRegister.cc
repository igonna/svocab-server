#include "UserRegister.h"

namespace v1
{
void UserRegister::registerWithoutEmail(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const {
    Json::Value ret;
    ret["message"] = "Hello, World!";
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);
}

void UserRegister::getCodeForEmailRegister(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const {

}

void UserRegister::registerEmail(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const {

}

void UserRegister::registerGoogle(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const {

}
}
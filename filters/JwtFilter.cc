#include "JwtFilter.h"
#include "utils/utils.h"

using namespace drogon;

void JwtFilter::doFilter(const HttpRequestPtr &req, FilterCallback &&fcb, FilterChainCallback &&fccb)
{
    auto token = req->getHeader("Authorization");

    try {
        if (!token.size()) {
            throw std::runtime_error("");
        }

        // get_secret_key
        auto secret_key = std::string();

        auto [user_id, device_num] = local_utils::token::decode_device_bare_token(token, secret_key);
        req->attributes()->insert("user_id", user_id);
        req->attributes()->insert("device_num", device_num);

        fccb();
    } catch (const std::exception& e) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
    }
}

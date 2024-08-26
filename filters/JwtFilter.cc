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

        auto [user_id, device_num, device_token] = local_utils::token::decode_client_refresh_token(token);
        req->attributes()->insert("user_id", user_id);
        req->attributes()->insert("device_num", device_num);
        req->attributes()->insert("device_token", device_token);

        fccb();
    } catch (const std::exception& e) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
    }
}

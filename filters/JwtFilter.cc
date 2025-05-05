#include "JwtFilter.h"
#include "utils/utils.h"
#include <drogon/HttpAppFramework.h>
#include "core/secret/secret_key.h"
#include "core/token/token.h"
#include "core/device/revoke.h"

using namespace drogon;

void JwtFilter::doFilter(const HttpRequestPtr &req, FilterCallback &&fcb, FilterChainCallback &&fccb) {
    auto token = req->getHeader("Authorization");

    drogon::async_run([this, req, fcb = std::move(fcb), fccb = std::move(fccb), token = std::move(token)]() mutable -> drogon::Task<> {
        static constexpr std::string redis_error = "redis_error";

        try {
            if (!token.size()) {
                throw std::runtime_error("");
            }

            // get_secret_key
            std::string secret_key;
            drogon::nosql::RedisClientPtr rc;
            bool is_device_revoked;
            try {
                rc = app().getRedisClient();
                secret_key = co_await core::secret::get_secret_key(rc);
            } catch (...) {
                throw std::runtime_error(redis_error);
            }

            auto [user_id, device_num, session_id] = core::token::decode_device_bare_token(token, secret_key);

            req->attributes()->insert(local_constants::attributes::user_id, user_id);
            req->attributes()->insert(local_constants::attributes::device_num, device_num);
            req->attributes()->insert(local_constants::attributes::session_id, session_id);

            is_device_revoked = co_await core::device::is_device_revoked(rc, user_id, device_num);

            if (is_device_revoked) {
                throw std::runtime_error("revoked");
            }

            auto users_db = app().getDbClient(local_constants::users_db);

            std::ignore = users_db->execSqlCoro(
                "update user_sessions set finished_at = NOW(), interactions = interactions + 1 where user_id = $1 and session_id = $2",
                user_id, session_id
            );

            fccb();
        } catch (const std::exception& e) {
            drogon::HttpStatusCode code;

            if (e.what() == redis_error) {
                code = k500InternalServerError;
            } else {
                code = k401Unauthorized;
            }

            LOG_ERROR << e.what();
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(code);
            fcb(resp);
        }
    });
}

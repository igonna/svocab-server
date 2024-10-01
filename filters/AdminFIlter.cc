#include "AdminFilter.h"
#include "utils/utils.h"
#include <drogon/HttpAppFramework.h>
#include "core/postgres/queries.h"

using namespace drogon;

void AdminFilter::doFilter(const HttpRequestPtr &req, FilterCallback &&fcb, FilterChainCallback &&fccb) {
    drogon::async_run([this, req, fcb = std::move(fcb), fccb = std::move(fccb)]() mutable -> drogon::Task<> {
        static constexpr std::string not_admin_error = "not_admin";

        try {
            auto users_db = app().getDbClient(local_constants::users_db);
            auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
            auto is_admin = co_await core::sql::is_user_of_sepcial_user_type(users_db, user_id, "admin");

            if (!is_admin) {
                throw std::runtime_error(not_admin_error);
            }
            fccb();
        } catch (const std::exception& e) {
            drogon::HttpStatusCode code;

            if (e.what() == not_admin_error) {
                code = k403Forbidden;
            } else {
                code = k500InternalServerError;
            }

            LOG_ERROR << e.what();
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(code);
            fcb(resp);
        }
    });
}

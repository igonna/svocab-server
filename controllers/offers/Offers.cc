#include "Offers.h"
#include "utils/constants.h"
#include "utils/utils.h"

namespace v1
{
Task<> Offers::setOfferPassed(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto offer_type_str = req->getHeader("offerType");

    if (offer_type_str.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    static const std::unordered_map<std::string, uint16_t> offer_types = {
        {"rate_us", 0}
    };

    if (!offer_types.contains(offer_type_str)) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "not_valid_offer_type");
    }

    auto offer_type = offer_types.at(offer_type_str);

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto cur_time = local_utils::time::get_unix_timestamp();

        auto result = co_await users_db->execSqlCoro(
            " insert into user_offers (user_id, offer_type, is_passed, created_at)"
            " values ($1, $2, $3, $4) on conflict do nothing", user_id, offer_type, true, cur_time
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("offer_not_created");
        }

        co_return local_utils::Response::OK(callback, "");
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}
}
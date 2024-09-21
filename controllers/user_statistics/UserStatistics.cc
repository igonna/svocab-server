#include "UserStatistics.h"
#include "utils/utils.h"

namespace v1
{
Task<> UserStatistics::getUserStatistics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    Json::Value result_body;

    auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

    auto& statistics = result_body["statistics"];
    statistics["learned_this_time"] = 0;
    statistics["learned_today"] = 0;
    statistics["repeated_today"] = 0;
    statistics["total_learned_words"] = 0;

    auto& performance = result_body["performance"];
    performance["learned_per_day_on_average"] = 0;
    performance["days_streak"] = 0;
    performance["average_focus_size"] = 0;

    result_body["rank"] = 0;

    co_return local_utils::Response::JSON_OK(callback, result_body);
}
}
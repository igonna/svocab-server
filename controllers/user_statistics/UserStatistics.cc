#include "UserStatistics.h"
#include "utils/utils.h"

namespace v1
{
Task<> UserStatistics::getUserStatistics(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    Json::Value result_body;

    auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
    
    // statistics
    //     learned_this_time
    //     learned_today
    //     repeated_today
    //     total_learned_words
    // performance
    //     learned_per_day_on_average
    //     days_streak
    //     average_focus_size

    // rank
    //     value


    co_return local_utils::Response::JSON_OK(callback, result_body);
}
}
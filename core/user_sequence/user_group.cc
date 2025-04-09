#include "core/user_sequence/user_group.h"
#include "utils/constants.h"
#include "utils/utils.h"

namespace core::user_sequence {

drogon::Task<UserGroup> UserGroup::get_group_info(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto result = co_await users_db->execSqlCoro(
        " SELECT"
            " ug.group_id, "
            " ug.group_size, "
            " ug.last_update_id, "
            " ug.last_gen_id,"
            " COUNT(ui.item_id) AS items_count,"
            " ug.last_interaction_time"
        " FROM user_group ug"
        " LEFT JOIN user_items ui"
            " ON ug.user_id = ui.user_id AND ug.group_id = ui.group_id"
        " WHERE ug.user_id = $1"
        " GROUP BY"
            " ug.group_id,"
            " ug.group_size,"
            " ug.last_update_id,"
            " ug.last_gen_id,"
            " ug.last_interaction_time", user_id
    );

    auto group_id = result[0]["group_id"].as<uint32_t>();
    auto group_size = result[0]["group_size"].as<uint32_t>();
    auto group_last_update_id = result[0]["last_update_id"].as<uint32_t>();
    auto group_last_gen_id = result[0]["last_gen_id"].as<uint32_t>();
    auto group_items_count = result[0]["items_count"].as<uint32_t>();
    auto last_interaction_time = result[0]["last_interaction_time"].as<uint32_t>();

    co_return UserGroup(
        group_id,
        group_size,
        group_last_update_id,
        group_last_gen_id,
        group_items_count,
        last_interaction_time
    );
}

bool UserGroup::is_user_added_new_content() {
    return last_update_id != last_gen_id;
}

bool UserGroup::is_group_expired() {
    auto time_now = local_utils::time::get_unix_timestamp();
    return time_now - last_interaction_time > local_constants::focus_group::focus_group_expire_seconds;
}
}
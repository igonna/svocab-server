#include "core/user_sequence/user_group.h"

namespace core::user_sequence {

drogon::Task<UserGroup> UserGroup::get_group_info(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto result = co_await users_db->execSqlCoro(
        " SELECT"
            " ug.group_id, "
            " ug.group_size, "
            " ug.last_update_id, "
            " ug.last_gen_id,"
            " COUNT(ui.item_id) AS items_count"
        " FROM user_group ug"
        " LEFT JOIN user_items ui"
            " ON ug.user_id = ui.user_id AND ug.group_id = ui.group_id"
        " WHERE ug.user_id = $1"
        " GROUP BY"
            " ug.group_id,"
            " ug.group_size,"
            " ug.last_update_id,"
            " ug.last_gen_id", user_id
    );

    auto group_id = result[0]["group_id"].as<uint32_t>();
    auto group_size = result[0]["group_size"].as<uint32_t>();
    auto group_last_update_id = result[0]["last_update_id"].as<uint32_t>();
    auto group_last_gen_id = result[0]["last_gen_id"].as<uint32_t>();
    auto group_items_count = result[0]["items_count"].as<uint32_t>();

    co_return UserGroup(
        group_id,
        group_size,
        group_last_update_id,
        group_last_gen_id,
        group_items_count
    );
}

}
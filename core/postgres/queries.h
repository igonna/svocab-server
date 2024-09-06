#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>

namespace core::sql {

static drogon::Task<> update_user_sequence_last_update_id(const drogon::orm::DbClientPtr& users_db, uint32_t user_id) {
    auto result = co_await users_db->execSqlCoro("update user_sequence set last_update_id = last_update_id + 1 where user_id = $1", user_id);
    if (!result.affectedRows()) {
        throw std::runtime_error("not_updated");
    }
    co_return;
}

}
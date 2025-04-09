#pragma once

#include <drogon/drogon.h>
#include <drogon/utils/coroutine.h>
#include <vector>
#include <cstdint>
#include "core/user_sequence_words/user_sequence_words.h"
#include "core/phrases_generation/phrases_generator.h"

namespace core::user_sequence {
    struct UserGroup {
        static drogon::Task<UserGroup> get_group_info(drogon::orm::DbClientPtr users_db, uint64_t user_id);
        bool is_user_added_new_content();
        bool is_group_expired();

        UserGroup(
            UserGroup&& other
        ) : group_id(other.group_id),
            group_size(other.group_size),
            last_update_id(other.last_update_id),
            last_gen_id(other.last_gen_id),
            items_count(other.items_count),
            last_interaction_time(other.last_interaction_time) {}

        UserGroup& operator=(UserGroup&& other) noexcept {
            if (this != &other) {
                group_id = other.group_id;
                group_size = other.group_size;
                last_update_id = other.last_update_id;
                last_gen_id = other.last_gen_id;
                items_count = other.items_count;
                last_interaction_time = other.last_interaction_time;
            }
            return *this;
        }

        UserGroup(
            uint32_t group_id,
            uint32_t group_size,
            uint32_t last_update_id,
            uint32_t last_gen_id,
            uint32_t items_count,
            uint32_t last_interaction_time
        ) : group_id(group_id),
            group_size(group_size),
            last_update_id(last_update_id),
            last_gen_id(last_gen_id),
            items_count(items_count),
            last_interaction_time(last_interaction_time) {}

        uint32_t group_id;
        uint32_t group_size;
        uint32_t last_update_id;
        uint32_t last_gen_id;
        uint32_t items_count;
        uint32_t last_interaction_time;
    };
}
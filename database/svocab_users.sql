-- CREATE TYPE SpecialUserType AS ENUM ('admin', 'tester');
CREATE TYPE SpecialUserType AS ENUM ('tester');

CREATE TABLE users (
    id BIGSERIAL PRIMARY KEY,
    email TEXT,
    special_user_type SpecialUserType,
    register_date TIMESTAMP NOT NULL DEFAULT NOW(),
    token_last_request_time INT NOT NULL DEFAULT 0, -- dead user detection
    last_device_num INT NOT NULL DEFAULT 0,
    UNIQUE (id, email)
) PARTITION BY HASH (id);

CREATE TABLE user_offers (
    user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
    offer_type SMALLINT NOT NULL,
    is_passed BOOLEAN NOT NULL DEFAULT FALSE,
    created_at INT NOT NULL,
    PRIMARY KEY (user_id, offer_type)
) PARTITION BY HASH (user_id);

CREATE TABLE last_user_special_offer_time (
    user_id BIGINT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    last_special_offer_time TIMESTAMP NOT NULL DEFAULT NOW()
) PARTITION BY HASH (user_id);

CREATE TABLE user_language_pairs (
    user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
    src_lang CHAR(2) NOT NULL,
    dst_lang CHAR(2) NOT NULL,
    is_enabled BOOLEAN NOT NULL DEFAULT TRUE,
    last_enable_time INT NOT NULL,
    PRIMARY KEY (user_id, src_lang, dst_lang)
) PARTITION BY HASH (user_id);

CREATE TABLE user_devices (
    user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
    device_num INT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),

    device_name TEXT NOT NULL DEFAULT '',
    device_model TEXT NOT NULL DEFAULT '',
    device_os_version TEXT NOT NULL DEFAULT '',

    device_refresh_token CHAR(16) NOT NULL,
    token_last_request_time INT NOT NULL DEFAULT 0,
    last_ip_address INT NOT NULL DEFAULT 0,
    PRIMARY KEY (user_id, device_num)
) PARTITION BY HASH (user_id);

CREATE TABLE user_words (
    user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
    src_lang CHAR(2) NOT NULL,
    word TEXT NOT NULL,

    is_learned BOOLEAN NOT NULL DEFAULT FALSE,
    is_hidden BOOLEAN NOT NULL DEFAULT FALSE,

    learn_confirmation_level SMALLINT NOT NULL DEFAULT 0,
    right_guess_count SMALLINT NOT NULL DEFAULT 0,
    wrong_guess_count SMALLINT NOT NULL DEFAULT 0,

    last_update_time INT NOT NULL DEFAULT 0,
    PRIMARY KEY (user_id, src_lang, word)
) PARTITION BY HASH (user_id);

CREATE TABLE user_limits (
    user_id BIGINT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,

    generated_phrases_count SMALLINT NOT NULL DEFAULT 0,
    last_generated_phrase_start_time INT NOT NULL DEFAULT 0,

    generated_topics_count SMALLINT NOT NULL DEFAULT 0,
    last_generated_topic_start_time INT NOT NULL DEFAULT 0,

    processed_files_count SMALLINT NOT NULL DEFAULT 0,
    last_processed_file_start_time INT NOT NULL DEFAULT 0
) PARTITION BY HASH (user_id);

-- CREATE TABLE user_sequence (
--     user_id BIGINT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
--     to_learn_words TEXT NOT NULL DEFAULT '', -- en;ru;word;1;0; -- src_lang;dst_lang;word;confirmation_level;is_passed

--     last_gen_id INT NOT NULL DEFAULT 0,
--     last_update_id INT NOT NULL DEFAULT 0,
--     last_gen_time INT NOT NULL DEFAULT 0,

--     focus_group_size INT NOT NULL DEFAULT 3,
--     focus_group JSONB NOT NULL DEFAULT '{}',
--     focus_group_id INT NOT NULL DEFAULT 0,
--     focus_group_last_update_time INT NOT NULL DEFAULT 0,
--     previous_focus_group_result JSONB NOT NULL DEFAULT '{}'
-- ) PARTITION BY HASH (user_id);

CREATE TABLE user_sequence (
    user_id BIGINT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    to_learn_words TEXT NOT NULL DEFAULT '', -- en;ru;word;1;0; -- src_lang;dst_lang;word;confirmation_level;is_passed

    last_gen_id INT NOT NULL DEFAULT 0,
    last_update_id INT NOT NULL DEFAULT 0,
    last_gen_time INT NOT NULL DEFAULT 0,

    focus_group_size INT NOT NULL DEFAULT 3,
    focus_group JSONB NOT NULL DEFAULT '{}',
    focus_group_id INT NOT NULL DEFAULT 0,
    focus_group_last_update_time INT NOT NULL DEFAULT 0
) PARTITION BY HASH (user_id);

-- CREATE TABLE user_topics (
--     topic_id BIGSERIAL,
--     user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,

--     src_lang CHAR(2) NOT NULL,
--     dst_lang CHAR(2) NOT NULL,

--     topic_title TEXT NOT NULL DEFAULT '',
--     mix_value REAL NOT NULL DEFAULT 1.0,
--     skip_value REAL NOT NULL DEFAULT 0.0,
--     position INT NOT NULL,
--     is_enabled BOOLEAN NOT NULL DEFAULT TRUE,

--     order_words_by SMALLINT NOT NULL DEFAULT 1, -- 0 first_appearance, 1 relative_popularity, 2 total_popularity
--     exclude_word_if_not_present_in_all_sources BOOLEAN NOT NULL DEFAULT FALSE,
--     exclude_word_if_very_rare BOOLEAN NOT NULL DEFAULT TRUE,

--     sources_metadata JSONB NOT NULL DEFAULT '{}',
--     sources_unprocessed JSONB NOT NULL DEFAULT '{}',
--     sources_processed JSONB NOT NULL DEFAULT '{}',

--     topic_words_sequence TEXT NOT NULL DEFAULT '',

--     last_source_id INT NOT NULL DEFAULT 0,

--     last_gen_id INT NOT NULL DEFAULT 0,
--     last_update_id INT NOT NULL DEFAULT 0,
--     PRIMARY KEY (topic_id, user_id)
-- ) PARTITION BY HASH (user_id);

CREATE TABLE user_topics (
    topic_id BIGSERIAL,
    user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,

    src_lang CHAR(2) NOT NULL,
    dst_lang CHAR(2) NOT NULL,

    created_at INT NOT NULL,
    last_update_time INT NOT NULL,
    is_enabled BOOLEAN NOT NULL DEFAULT TRUE,

    title TEXT NOT NULL,
    origin TEXT NOT NULL,
    origin_type TEXT NOT NULL,
    words_sequence TEXT NOT NULL,
    PRIMARY KEY (topic_id, user_id)
) PARTITION BY HASH (user_id);

-- CREATE TABLE global_topics (
--     topic_id SERIAL PRIMARY KEY,
--     src_lang CHAR(2),
--     src_title_en TEXT NOT NULL,
--     default_position INT NOT NULL,
--     words TEXT NOT NULL,
--     UNIQUE(topic_id, src_lang)
-- );

-- CREATE TABLE user_global_topics (
--     user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
--     topic_id INT REFERENCES global_topics(topic_id) ON DELETE CASCADE,
--     src_lang CHAR(2) NOT NULL,
--     dst_lang CHAR(2) NOT NULL,
--     dst_title TEXT,
--     mix_value REAL NOT NULL,
--     skip_value REAL NOT NULL,
--     position INT NOT NULL,
--     is_enabled BOOLEAN NOT NULL DEFAULT TRUE,
--     PRIMARY KEY (user_id, topic_id, dst_lang)
-- ) PARTITION BY HASH (user_id);

-- CREATE TABLE ranks (
--     rank_pos SMALLINT PRIMARY KEY,
--     rank_min_score BIGINT
-- );

-- CREATE TABLE user_statistics (
--     user_id BIGINT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
--     learned_this_time INT NOT NULL DEFAULT 0,
--     last_learn_time INT NOT NULL DEFAULT 0,
--     learned_today INT NOT NULL DEFAULT 0,
--     learned_words_total INT NOT NULL DEFAULT 0,
--     learn_streak INT NOT NULL DEFAULT 0,
--     total_learn_days INT NOT NULL DEFAULT 0,
--     last_repeat_time INT NOT NULL DEFAULT 0,
--     repeated_today INT NOT NULL DEFAULT 0,
--     focus_size_total_sum BIGINT NOT NULL DEFAULT 0,
--     focus_size_changes_total_count INT NOT NULL DEFAULT 0,
--     user_rank SMALLINT NOT NULL DEFAULT 100, -- from 0 to 100
--     last_rank_update_time INT NOT NULL DEFAULT 0
-- ) PARTITION BY HASH (user_id);

CREATE TABLE user_statistics (
    user_id BIGINT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    in_learning_count INT NOT NULL DEFAULT 0,
    mastered_count INT NOT NULL DEFAULT 0
) PARTITION BY HASH (user_id);

CREATE TABLE user_google_play_subscription ( -- to do this, after more details known
    user_id BIGINT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    subscription_id VARCHAR(255) NOT NULL,
    purchase_token VARCHAR(255) NOT NULL,
    purchase_time TIMESTAMP NOT NULL,
    expiry_date TIMESTAMP,
    is_active BOOLEAN DEFAULT TRUE,
    last_verify_date TIMESTAMP NOT NULL,
    last_notification_code TEXT NOT NULL,
    expiry_time INT NOT NULL
) PARTITION BY HASH (user_id);

CREATE TABLE users_p0 PARTITION OF users FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE users_p1 PARTITION OF users FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE users_p2 PARTITION OF users FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE users_p3 PARTITION OF users FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE UNIQUE INDEX idx_unique_email_p0 ON users_p0 (id, email);
CREATE UNIQUE INDEX idx_unique_email_p1 ON users_p1 (id, email);
CREATE UNIQUE INDEX idx_unique_email_p2 ON users_p2 (id, email);
CREATE UNIQUE INDEX idx_unique_email_p3 ON users_p3 (id, email);

CREATE TABLE user_sequence_p0 PARTITION OF user_sequence FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_sequence_p1 PARTITION OF user_sequence FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_sequence_p2 PARTITION OF user_sequence FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_sequence_p3 PARTITION OF user_sequence FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_limits_p0 PARTITION OF user_limits FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_limits_p1 PARTITION OF user_limits FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_limits_p2 PARTITION OF user_limits FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_limits_p3 PARTITION OF user_limits FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_offers_p0 PARTITION OF user_offers FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_offers_p1 PARTITION OF user_offers FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_offers_p2 PARTITION OF user_offers FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_offers_p3 PARTITION OF user_offers FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE last_user_special_offer_time_p0 PARTITION OF last_user_special_offer_time FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE last_user_special_offer_time_p1 PARTITION OF last_user_special_offer_time FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE last_user_special_offer_time_p2 PARTITION OF last_user_special_offer_time FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE last_user_special_offer_time_p3 PARTITION OF last_user_special_offer_time FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_language_pairs_p0 PARTITION OF user_language_pairs FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_language_pairs_p1 PARTITION OF user_language_pairs FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_language_pairs_p2 PARTITION OF user_language_pairs FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_language_pairs_p3 PARTITION OF user_language_pairs FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_devices_p0 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_devices_p1 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_devices_p2 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_devices_p3 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_words_p0 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_words_p1 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_words_p2 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_words_p3 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_topics_p0 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_topics_p1 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_topics_p2 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_topics_p3 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_statistics_p0 PARTITION OF user_statistics FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_statistics_p1 PARTITION OF user_statistics FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_statistics_p2 PARTITION OF user_statistics FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_statistics_p3 PARTITION OF user_statistics FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_google_play_subscription_p0 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_google_play_subscription_p1 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_google_play_subscription_p2 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_google_play_subscription_p3 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 3);
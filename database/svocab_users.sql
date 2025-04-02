CREATE TYPE SpecialUserType AS ENUM ('tester');

CREATE TABLE users (
    id BIGSERIAL PRIMARY KEY,
    email TEXT,
    special_user_type SpecialUserType,
    register_date TIMESTAMP NOT NULL DEFAULT NOW(),
    token_last_request_time INT NOT NULL DEFAULT 0, -- dead user detection
    last_device_num INT NOT NULL DEFAULT 0,
    is_deleted BOOLEAN NOT NULL DEFAULT FALSE,
    delete_date TIMESTAMP,
    email_before_delete TEXT,
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

CREATE TABLE user_devices (
    user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
    device_num INT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),

    device_name TEXT NOT NULL DEFAULT '',
    device_model TEXT NOT NULL DEFAULT '',
    device_os_version TEXT NOT NULL DEFAULT '',

    device_refresh_token CHAR(16),
    token_last_request_time INT NOT NULL DEFAULT 0,
    last_ip_address INT NOT NULL DEFAULT 0,
    PRIMARY KEY (user_id, device_num)
) PARTITION BY HASH (user_id);

CREATE TABLE user_sessions (
    session_id BIGSERIAL,
    user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
    started_at INT NOT NULL,
    finished_at INT NOT NULL,
    ip_address INT NOT NULL,
    interactions INT NOT NULL DEFAULT 0,
    PRIMARY KEY (session_id, user_id)
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

CREATE TABLE user_language_pairs (
    user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
    src_lang CHAR(2) NOT NULL,
    dst_lang CHAR(2) NOT NULL,
    is_enabled BOOLEAN NOT NULL DEFAULT TRUE,
    last_enable_time INT NOT NULL,
    PRIMARY KEY (user_id, src_lang, dst_lang)
) PARTITION BY HASH (user_id);

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

CREATE TABLE user_group (
    user_id BIGINT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    group_id INT NOT NULL DEFAULT 0,
    group_size INT NOT NULL DEFAULT 3,

    last_update_id INT NOT NULL DEFAULT 0,
    last_gen_id INT NOT NULL DEFAULT 0
) PARTITION BY HASH (user_id);

CREATE TABLE user_items (
    user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
    group_id INT NOT NULL,
    item_id SMALLINT NOT NULL,
    is_passed BOOLEAN NOT NULL DEFAULT FALSE,
    is_hidden BOOLEAN NOT NULL DEFAULT FALSE,
    created_at INT NOT NULL,
    passed_at INT,

    item_type SMALLINT NOT NULL,

    src_lang CHAR(2),
    dst_lang CHAR(2),
    word_phrase_id INT,

    mistakes_count SMALLINT NOT NULL DEFAULT 0,
    seconds_spent SMALLINT NOT NULL DEFAULT 0,
    seconds_spent_on_answer SMALLINT NOT NULL DEFAULT 0,
    PRIMARY KEY (user_id, group_id, item_id)
) PARTITION BY HASH (user_id);

CREATE TABLE user_words (
    user_id BIGINT REFERENCES users(id) ON DELETE CASCADE,
    src_lang CHAR(2) NOT NULL,
    word TEXT NOT NULL,

    is_learned BOOLEAN NOT NULL DEFAULT FALSE,
    is_hidden BOOLEAN NOT NULL DEFAULT FALSE,

    learn_confirmation_level SMALLINT NOT NULL DEFAULT 0,

    last_update_time INT NOT NULL DEFAULT 0,
    PRIMARY KEY (user_id, src_lang, word)
) PARTITION BY HASH (user_id);

CREATE TABLE user_google_play_subscription (
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

CREATE TABLE user_offers_p0 PARTITION OF user_offers FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_offers_p1 PARTITION OF user_offers FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_offers_p2 PARTITION OF user_offers FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_offers_p3 PARTITION OF user_offers FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE last_user_special_offer_time_p0 PARTITION OF last_user_special_offer_time FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE last_user_special_offer_time_p1 PARTITION OF last_user_special_offer_time FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE last_user_special_offer_time_p2 PARTITION OF last_user_special_offer_time FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE last_user_special_offer_time_p3 PARTITION OF last_user_special_offer_time FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_devices_p0 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_devices_p1 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_devices_p2 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_devices_p3 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_sessions_p0 PARTITION OF user_sessions FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_sessions_p1 PARTITION OF user_sessions FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_sessions_p2 PARTITION OF user_sessions FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_sessions_p3 PARTITION OF user_sessions FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_limits_p0 PARTITION OF user_limits FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_limits_p1 PARTITION OF user_limits FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_limits_p2 PARTITION OF user_limits FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_limits_p3 PARTITION OF user_limits FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_language_pairs_p0 PARTITION OF user_language_pairs FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_language_pairs_p1 PARTITION OF user_language_pairs FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_language_pairs_p2 PARTITION OF user_language_pairs FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_language_pairs_p3 PARTITION OF user_language_pairs FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_topics_p0 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_topics_p1 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_topics_p2 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_topics_p3 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_group_p0 PARTITION OF user_group FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_group_p1 PARTITION OF user_group FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_group_p2 PARTITION OF user_group FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_group_p3 PARTITION OF user_group FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_items_p0 PARTITION OF user_items FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_items_p1 PARTITION OF user_items FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_items_p2 PARTITION OF user_items FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_items_p3 PARTITION OF user_items FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_words_p0 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_words_p1 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_words_p2 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_words_p3 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_google_play_subscription_p0 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_google_play_subscription_p1 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_google_play_subscription_p2 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_google_play_subscription_p3 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 3);
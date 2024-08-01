CREATE TYPE SpecialUserType AS ENUM ('admin', 'tester');

CREATE TABLE user (
    id SERIAL PRIMARY KEY,
    email TEXT NOT NULL,
    special_user_type SpecialUserType,
    register_date TIMESTAMP NOT NULL DEFAULT NOW(),
    subscription_last_verify_time INT NOT NULL DEFAULT 0,
    token_last_request_time INT NOT NULL DEFAULT 0, -- dead user detection
    last_device_num INT NOT NULL DEFAULT 0,
    UNIQUE (email)
) PARTITION BY HASH (id);

CREATE TABLE user_p0 PARTITION OF user FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_p1 PARTITION OF user FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_p2 PARTITION OF user FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_p3 PARTITION OF user FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_state (
    user_id INT REFERENCES user(id) ON DELETE CASCADE,
    has_subscription BOOLEAN NOT NULL DEFAULT FALSE,
    topics_last_update_time INT NOT NULL DEFAULT 0,
    check_points_last_update_time INT NOT NULL DEFAULT 0,
    learned_words_last_update_time INT NOT NULL DEFAULT 0,
    learned_words_hot_sync_last_update_time INT NOT NULL DEFAULT 0,
    focus_group TEXT NOT NULL DEFAULT '', --word;2;t;\n
    selected_languages TEXT NOT NULL DEFAULT '', -- en;es;1.25;t\n
) PARTITION BY HASH (user_id);

CREATE TABLE user_state_p0 PARTITION OF user_state FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_state_p1 PARTITION OF user_state FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_state_p2 PARTITION OF user_state FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_state_p3 PARTITION OF user_state FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_check_points (
    user_id INT PRIMARY KEY REFERENCES user(id) ON DELETE CASCADE,
    principles_tutorial_passed BOOLEAN NOT NULL DEFAULT FALSE,
    flow_tutorial_passed BOOLEAN NOT NULL DEFAULT FALSE,
    filter_tutorial_passed BOOLEAN NOT NULL DEFAULT FALSE,
    rate_us_passed BOOLEAN NOT NULL DEFAULT FALSE
) PARTITION BY HASH (user_id);

CREATE TABLE user_check_points_p0 PARTITION OF user_check_points FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_check_points_p1 PARTITION OF user_check_points FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_check_points_p2 PARTITION OF user_check_points FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_check_points_p3 PARTITION OF user_check_points FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_devices (
    user_id INT REFERENCES user(id) ON DELETE CASCADE,
    device_num INT,
    os_name TEXT NOT NULL,
    os_version TEXT NOT NULL,
    device_market_name TEXT NOT NULL,
    device_model TEXT NOT NULL,
    device_refresh_token TEXT NOT NULL,
    token_last_request_time INT NOT NULL,
    created_at INT NOT NULL,
    ip_address INT NOT NULL,
    PRIMARY KEY (user_id, device_num)
) PARTITION BY HASH (user_id);

CREATE TABLE user_devices_p0 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_devices_p1 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_devices_p2 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_devices_p3 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_learned_words (
    user_id INT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
    lang_tag CHAR(2) NOT NULL,
    words JSONB NOT NULL DEFAULT '{}',
    PRIMARY KEY (user_id, lang_tag)
) PARTITION BY HASH (user_id);

CREATE TABLE user_learned_words_p0 PARTITION OF user_learned_words FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_learned_words_p1 PARTITION OF user_learned_words FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_learned_words_p2 PARTITION OF user_learned_words FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_learned_words_p3 PARTITION OF user_learned_words FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_learned_words_hot_sync (
    user_id INT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
    lang_tag CHAR(2) NOT NULL,
    words JSONB NOT NULL DEFAULT '{}',
    PRIMARY KEY (user_id, lang_tag)
) PARTITION BY HASH (user_id);

CREATE TABLE user_learned_words_hot_sync_p0 PARTITION OF user_learned_words_hot_sync FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_learned_words_hot_sync_p1 PARTITION OF user_learned_words_hot_sync FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_learned_words_hot_sync_p2 PARTITION OF user_learned_words_hot_sync FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_learned_words_hot_sync_p3 PARTITION OF user_learned_words_hot_sync FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_google_play_subscrition ( -- to do this, after more details known
    user_id INT PRIMARY KEY REFERENCES user(id) ON DELETE CASCADE,
    subscription_id VARCHAR(255) NOT NULL,
    purchase_token VARCHAR(255) NOT NULL,
    purchase_time TIMESTAMP NOT NULL,
    expiry_date TIMESTAMP,
    is_active BOOLEAN DEFAULT TRUE,
    last_verify_date TIMESTAMP NOT NULL,
    last_notification_code TEXT NOT NULL,
    expiry_time INT NOT NULL
);

CREATE TABLE words_progress_cold_sync (
    user_id SERIAL NOT NULL REFERENCES user(id) ON DELETE CASCADE,
    lang_tag CHAR(2) NOT NULL,
    last_sync_time TIMESTAMP NOT NULL DEFAULT NOW(),
    words JSONB NOT NULL DEFAULT '{}',
    UNIQUE (user_id, lang_tag)
);

CREATE TABLE words_progress_hot_sync (
    user_id SERIAL NOT NULL REFERENCES user(id) ON DELETE CASCADE,
    lang_tag CHAR(2) NOT NULL,
    last_sync_time TIMESTAMP NOT NULL DEFAULT NOW(),
    words JSONB NOT NULL DEFAULT '{}',
    UNIQUE (user_id, lang_tag)
);

CREATE TABLE user_topic (
    user_id SERIAL NOT NULL REFERENCES user(id) ON DELETE CASCADE,
    topic_id BIGINT NOT NULL,
    metadata JSONB NOT NULL,
    last_update_time TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE(user_id, topic_id)
);


CREATE TABLE topic (
    topic_id SERIAL NOT NULL,
    src_lang CHAR(2) NOT NULL,
    metadata JSONB NOT NULL,
    last_update_time INT NOT NULL DEFAULT 0,
    UNIQUE(topic_id, src_lang)
);
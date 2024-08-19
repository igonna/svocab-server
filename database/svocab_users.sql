CREATE TYPE SpecialUserType AS ENUM ('admin', 'tester');

CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    email TEXT,
    special_user_type SpecialUserType,
    register_date TIMESTAMP NOT NULL DEFAULT NOW(),
    subscription_last_verify_time INT,
    token_last_request_time INT NOT NULL DEFAULT 0, -- dead user detection
    last_device_num INT NOT NULL DEFAULT 0,
    UNIQUE (id, email)
) PARTITION BY HASH (id);

CREATE TABLE user_sequence (
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    not_learned_words TEXT[], -- 200 words by default, but brough_back could increase, some of them could be NULL
    brought_back_words TEXT[], -- words which user removed from learned or hidden words

    was_cropped BOOLEAN NOT NULL DEFAULT FALSE,
    lowest_not_learned_pointer INT NOT NULL DEFAULT 0,

    last_gen_for_repeat_time INT NOT NULL DEFAULT 0, -- required to update every 6 hours, to mix words for repeat
    last_gen_id INT NOT NULL DEFAULT 0, -- current state
    last_update_id INT NOT NULL DEFAULT 0, -- >= last_gen_id

    last_focus_group TEXT,
    last_focus_group_gen_id INT NOT NULL DEFAULT 0
    PRIMARY KEY (user_id)
) PARTITION BY HASH (user_id);

CREATE TABLE user_state (
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    has_subscription BOOLEAN NOT NULL DEFAULT FALSE,
    check_points SMALLINT NOT NULL DEFAULT 0
) PARTITION BY HASH (user_id);

-- check_points:
-- first_lang_pair_created
-- principles_tutorial_passed
-- flow_tutorial_passed
-- filter_tutorial_passed
-- rate_us_passed

CREATE TABLE user_selected_languages (
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    selected_languages TEXT NOT NULL DEFAULT '', -- en;es;1.25;t\n
    PRIMARY KEY (user_id)
) PARTITION BY HASH (user_id);

CREATE TABLE user_devices (
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    device_num INT,
    created_at INT NOT NULL,
    os_name TEXT NOT NULL,
    os_version TEXT NOT NULL,
    device_market_name TEXT NOT NULL,
    device_model TEXT NOT NULL,
    device_refresh_token TEXT NOT NULL,
    token_last_request_time INT NOT NULL,
    last_ip_address INT NOT NULL,
    PRIMARY KEY (user_id, device_num)
) PARTITION BY HASH (user_id);

CREATE TABLE user_words (
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    lang_tag CHAR(2) NOT NULL,
    words TEXT NOT NULL DEFAULT '', -- word;16;160010120
    PRIMARY KEY (user_id, lang_tag)
) PARTITION BY HASH (user_id);

CREATE TABLE user_topics (
    topic_id SERIAL,
    user_id INT REFERENCES users(id) ON DELETE CASCADE,

    topic_metadata TEXT NOT NULL,

    src_lang CHAR(2) NOT NULL,
    dst_lang CHAR(2) NOT NULL,

    topic_title TEXT,
    mix_value REAL NOT NULL DEFAULT 1.0,
    skip_value REAL NOT NULL DEFAULT 0.0,
    position INT NOT NULL,
    is_enabled BOOLEAN NOT NULL DEFAULT TRUE,

    word_count_type SMALLINT NOT NULL DEFAULT 0, -- balanced_sources, summarized_sources
    word_order_type SMALLINT NOT NULL DEFAULT 0, -- popularity, first_appearance
    exclude_words_that_not_in_all_sources BOOLEAN NOT NULL DEFAULT FALSE,

    texts_sources TEXT[],

    texts_processed TEXT[],
    documents_processed TEXT[],
    website_urls_processed TEXT[],
    youtube_urls_processed TEXT[],

    texts_titles TEXT[],
    documents_titles TEXT[],
    website_urls_titles TEXT[],
    youtube_urls_titles TEXT[],

    texts_fail_title_status TEXT[], -- 'text_title','status'
    documents_fail_title_status TEXT[],
    website_urls_fail_title_status TEXT[],
    youtube_urls_fail_title_status TEXT[],

    topic_processed_words TEXT NOT NULL,

    last_gen_id INT NOT NULL,
    last_update_id INT NOT NULL,
    PRIMARY KEY (topic_id, user_id)
) PARTITION BY HASH (user_id);

CREATE TABLE user_custom_global_topic_options (
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    topic_id INT,
    mix_value REAL NOT NULL,
    skip_value REAL NOT NULL,
    position INT NOT NULL,
    is_enabled BOOLEAN NOT NULL,
    PRIMARY KEY (user_id, topic_id)
) PARTITION BY HASH (user_id);

CREATE TABLE ranks (
    rank_pos PRIMARY KEY SMALLINT,
    rank_min_score BIGINT,
    last_rank_update INT,
);

CREATE TABLE user_statistics (
    user_id INT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    app_sessions JSONB NOT NULL, -- i am not sure that we need it
    last_learn_time INT NOT NULL DEFAULT 0,
    learned_today INT NOT NULL DEFAULT 0,
    learned_words_total INT NOT NULL DEFAULT 0,
    learn_streak INT NOT NULL DEFAULT 0,
    total_learn_days INT NOT NULL DEFAULT 0,
    last_repeat_time INT NOT NULL DEFAULT 0,
    repeated_today INT NOT NULL DEFAULT 0,
    focus_size_total_sum BIGINT NOT NULL DEFAULT 0,
    focus_size_changes_total_count INT NOT NULL DEFAULT 0,
    user_rank SMALLINT NOT NULL DEFAULT 100, -- from 0 to 100
    last_rank_update_time INT NOT NULL DEFAULT 0
) PARTITION BY HASH (user_id);

CREATE INDEX idx_score ON user_statistics (user_score DESC);

CREATE TABLE user_google_play_subscription ( -- to do this, after more details known
    user_id INT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
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

CREATE UNIQUE INDEX idx_unique_email ON users (id, email);

CREATE TABLE user_state_p0 PARTITION OF user_state FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_state_p1 PARTITION OF user_state FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_state_p2 PARTITION OF user_state FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_state_p3 PARTITION OF user_state FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_selected_languages_p0 PARTITION OF user_selected_languages FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_selected_languages_p1 PARTITION OF user_selected_languages FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_selected_languages_p2 PARTITION OF user_selected_languages FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_selected_languages_p3 PARTITION OF user_selected_languages FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_devices_p0 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_devices_p1 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_devices_p2 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_devices_p3 PARTITION OF user_devices FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_words_p0 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_words_p1 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_words_p2 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_words_p3 PARTITION OF user_words FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_hidden_words_p0 PARTITION OF user_hidden_words FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_hidden_words_p1 PARTITION OF user_hidden_words FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_hidden_words_p2 PARTITION OF user_hidden_words FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_hidden_words_p3 PARTITION OF user_hidden_words FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_topics_p0 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_topics_p1 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_topics_p2 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_topics_p3 PARTITION OF user_topics FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_global_topic_metadata_p0 PARTITION OF user_global_topic_metadata FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_global_topic_metadata_p1 PARTITION OF user_global_topic_metadata FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_global_topic_metadata_p2 PARTITION OF user_global_topic_metadata FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_global_topic_metadata_p3 PARTITION OF user_global_topic_metadata FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_statistics_p0 PARTITION OF user_statistics FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_statistics_p1 PARTITION OF user_statistics FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_statistics_p2 PARTITION OF user_statistics FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_statistics_p3 PARTITION OF user_statistics FOR VALUES WITH (MODULUS 4, REMAINDER 3);

CREATE TABLE user_google_play_subscription_p0 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 0);
CREATE TABLE user_google_play_subscription_p1 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 1);
CREATE TABLE user_google_play_subscription_p2 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 2);
CREATE TABLE user_google_play_subscription_p3 PARTITION OF user_google_play_subscription FOR VALUES WITH (MODULUS 4, REMAINDER 3);
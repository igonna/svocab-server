CREATE EXTENSION "uuid-ossp";

CREATE TYPE UserType AS ENUM ('common', 'admin', 'tester');

CREATE TABLE user (
    id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
    email TEXT NOT NULL,
    user_type UserType NOT NULL DEFAULT 'common',
    register_date TIMESTAMP NOT NULL DEFAULT NOW(),
    token_last_request_date TIMESTAMP, -- dead user detection
    devices JSONB NOT NULL DEFAULT '{}',
    UNIQUE (email)
);

CREATE TABLE words_progress_cold_sync (
    user_id UUID NOT NULL REFERENCES user(id) ON DELETE CASCADE,
    lang_tag CHAR(2) NOT NULL,
    last_sync_time TIMESTAMP NOT NULL DEFAULT NOW(),
    words JSONB NOT NULL DEFAULT '{}',
    UNIQUE (user_id, lang_tag)
);

CREATE TABLE words_progress_hot_sync (
    user_id UUID NOT NULL REFERENCES user(id) ON DELETE CASCADE,
    lang_tag CHAR(2) NOT NULL,
    last_sync_time TIMESTAMP NOT NULL DEFAULT NOW(),
    words JSONB NOT NULL DEFAULT '{}',
    UNIQUE (user_id, lang_tag)
);

CREATE TABLE selections_cold_sync (
    user_id UUID NOT NULL REFERENCES user(id) ON DELETE CASCADE,
    last_sync_time TIMESTAMP NOT NULL DEFAULT NOW(),
    data JSONB NOT NULL DEFAULT '{}',
    UNIQUE (user_id)
);

CREATE TABLE selections_hot_sync (
    user_id UUID NOT NULL REFERENCES user(id) ON DELETE CASCADE,
    last_sync_time TIMESTAMP NOT NULL DEFAULT NOW(),
    data JSONB NOT NULL DEFAULT '{}',
    UNIQUE (user_id)
);

CREATE TABLE user_topic (
    topic_id SERIAL NOT NULL,
    origin_lang_tag CHAR(2) NOT NULL,
    title TEXT NOT NULL,
    popularity INT NOT NULL,
    source_content JSONB NOT NULL,
    content JSONB NOT NULL,
    last_update_time TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE(topic_id, origin_lang_tag)
);

CREATE TABLE user_topic_translation (
    topic_id SERIAL NOT NULL REFERENCES topic(topic_id),
    translation_lang_tag CHAR(2) NOT NULL,
    title_translation TEXT NOT NULL,
    content_translation JSONB NOT NULL,
    last_update_time TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE(topic_id, translation_lang_tag)
);
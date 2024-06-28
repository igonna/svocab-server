CREATE SCHEMA vocab_data;

CREATE TABLE vocab_data.languages (
    lang_tag CHAR(2) PRIMARY KEY, -- ISO 639-1
    popularity SMALLINT NOT NULL,
    title_translations JSONB NOT NULL,
    cur_version INT NOT NULL DEFAULT 0
);

CREATE TYPE translation_type AS ENUM ('one-to-many', 'one-to-one');

CREATE TABLE vocab_data.topics (
    topic_tag VARCHAR(100) PRIMARY KEY,
    translation_type translation_type NOT NULL,
    title_translations JSONB NOT NULL,
    supported_langs CHAR(2)[] NOT NULL,
    popularity SMALLINT NOT NULL,
    free_content SMALLINT NOT NULL DEFAULT 0,
    cur_version INT NOT NULL DEFAULT 0
);

CREATE TABLE vocab_data.topic_content (
    topic_tag VARCHAR(100) NOT NULL REFERENCES vocab_data.topics(topic_tag),
    lang_tag CHAR(2) NOT NULL REFERENCES vocab_data.languages(lang_tag),
    content_parts TEXT[] NOT NULL,
    content_parts_sizes INT[] NOT NULL,
    cur_version INT NOT NULL DEFAULT 0,
    UNIQUE(topic_tag, lang_tag)
);

CREATE TABLE vocab_data.topic_content_translation (
    topic_tag VARCHAR(30) NOT NULL REFERENCES vocab_data.topics(topic_tag),
    lang_tag CHAR(2) NOT NULL REFERENCES vocab_data.languages(lang_tag),
    lang_translation_tag CHAR(2) NOT NULL REFERENCES vocab_data.languages(lang_tag),
    content_parts TEXT[] NOT NULL,
    cur_version INT NOT NULL DEFAULT 0,
    UNIQUE(topic_tag, lang_tag, lang_translation_tag)
);

CREATE SCHEMA user_data;

CREATE EXTENSION "uuid-ossp";

CREATE TABLE user_data.users (
    id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
    email TEXT NOT NULL,
    register_date TIMESTAMP NOT NULL DEFAULT NOW(),
    token_last_request_date TEXT, -- dead user detection
    last_device_num INTEGER DEFAULT 0,
    devices JSONB NOT NULL DEFAULT '{}',
    UNIQUE (email)
);

CREATE TABLE user_data.user_keys (
    user_id UUID NOT NULL REFERENCES user_data.users(id) ON DELETE CASCADE,
);

CREATE SCHEMA progress_data;

CREATE TABLE progress_data.words_progress (
    user_id UUID NOT NULL REFERENCES user_data.users(id) ON DELETE CASCADE,
    lang_tag CHAR(2) NOT NULL REFERENCES vocab_data.languages(lang_tag),
    updated_devices_nums INTEGER[] NOT NULL,
    last_transaction_time TIMESTAMP NOT NULL DEFAULT NOW(),
    words JSONB NOT NULL DEFAULT '{}',
    UNIQUE (user_id, lang_tag)
);

CREATE TABLE progress_data.selections (
    user_id UUID NOT NULL REFERENCES user_data.users(id) ON DELETE CASCADE,
    updated_devices_nums INTEGER[] NOT NULL,
    data JSONB NOT NULL DEFAULT '{}',
    UNIQUE (user_id)
);

CREATE TYPE transaction_type AS ENUM ('progress', 'selections');

CREATE TABLE progress_data.transactions (
    id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES user_data.users(id) ON DELETE CASCADE,
    creation_time TIMESTAMP DEFAULT NOW(),
    update_device_id INTEGER NOT NULL,
    update_type transaction_type NOT NULL,
    update_data JSONB
);

-- CREATE TABLE devices_model_to_marketing_name_map (
--     model_name VARCHAR(30) PRIMARY KEY,
--     marketing_name VARCHAR(50) NOT NULL
-- );

-- CREATE SCHEMA payment_data;

-- CREATE TYPE payment_data.subscription_type AS ENUM (
--     'monthly', -- 7 dollars
--     'every_3_month', -- 19 dollars
--     'yearly', -- 99 dollars
--     'family' -- 179 dollars, and 3 person can use it for year
-- );

-- CREATE TABLE payment_data.subscription_plans (
--     id SERIAL PRIMARY KEY,
--     currency_code VARCHAR(3),
--     price NUMERIC(10, 2),
--     duration INTERVAL DAY NOT NULL,
--     subscription_type payment_data.subscription_type NOT NULL
-- );

-- CREATE TABLE payment_data.subscription_descriptions (
--     id SERIAL PRIMARY KEY,
--     subscription_plan_id INTEGER NOT NULL REFERENCES payment_data.subscription_plans(id),
--     lang_tag CHAR(2) NOT NULL REFERENCES vocab_data.languages(id),
--     name TEXT NOT NULL,
--     description TEXT NOT NULL
-- );

-- CREATE TYPE payment_data.activation_key_share_type AS ENUM (
--     'single',
--     'multiple'
-- );

-- CREATE TABLE payment_data.activation_keys (
--     id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
--     activations_left SMALLINT CHECK (activations_left >= 0),
--     share_type payment_data.activation_key_share_type NOT NULL
--        max people share limit
-- );

-- CREATE TABLE payment_data.user_subscriptions (
--     id SERIAL PRIMARY KEY,
--     user_id UUID NOT NULL REFERENCES user_data.users(id),
--     subscription_plan_id INTEGER NOT NULL REFERENCES payment_data.subscription_plans(id),
--     start_date TIMESTAMP NOT NULL,
--     expiration_date TIMESTAMP NOT NULL
--     -- auto_renewal BOOLEAN DEFAULT false,
--     --payment_info,
-- );
CREATE TABLE supported_langs (
    origin_lang_tag CHAR(2) PRIMARY KEY, -- ISO 639-1
    supported_translation_langs TEXT NOT NULL
);

CREATE TABLE topic (
    topic_id SERIAL NOT NULL,
    origin_lang_tag CHAR(2) NOT NULL,
    title TEXT NOT NULL,
    popularity INT NOT NULL,
    content TEXT NOT NULL,
    last_update_time TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE(topic_id, origin_lang_tag)
);

CREATE TABLE topic_translation (
    topic_id SERIAL NOT NULL REFERENCES topic(topic_id),
    translation_lang_tag CHAR(2) NOT NULL,
    title_translation TEXT NOT NULL,
    content_translation TEXT NOT NULL,
    last_update_time TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE(topic_id, translation_lang_tag)
);
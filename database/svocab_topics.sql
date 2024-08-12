CREATE TABLE topic (
    topic_id SERIAL,
    src_lang CHAR(2),
    topic_metadata TEXT NOT NULL,
    topic_words TEXT NOT NULL,
    last_update_id INT NOT NULL DEFAULT 0,
    UNIQUE(topic_id, src_lang)
);

CREATE TABLE topic_title_en (
    title TEXT NOT NULL,
    dst_lang CHAR(2),
    title_translation TEXT NOT NULL,
    PRIMARY KEY (title, dst_lang)
) PARTITION BY LIST (dst_lang);

-- create partition table for each dst_lang
CREATE TABLE topic_title_en_es PARTITION OF topic_title_en
FOR VALUES IN ('es');

-- create index (word, dst_lang) for each dst_lang
CREATE INDEX idx_topic_title_en_es_title_dst_lang
ON topic_title_en_es (title, dst_lang);
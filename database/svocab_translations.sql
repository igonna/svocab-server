-- create table for each src_lang
CREATE TABLE word_phrase_en (
    id SERIAL NOT NULL,
    word TEXT NOT NULL,
    dst_lang CHAR(2) NOT NULL,
    phrase TEXT NOT NULL,
    word_translation TEXT NOT NULL,
    phrase_translation TEXT NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    seconds_spent REAL NOT NULL,
    attempts SMALLINT NOT NULL
    PRIMARY KEY (word, dst_lang)
) PARTITION BY LIST (dst_lang);

-- create partition table for each dst_lang
CREATE TABLE word_phrase_en_es PARTITION OF word_phrase_en
FOR VALUES IN ('es');

-- create index (word, dst_lang) for each dst_lang
CREATE INDEX idx_word_phrase_en_es_word_dst_lang
ON word_phrase_en_es (word, dst_lang);

-- create index (word_translation, dst_lang) for each dst_lang
CREATE INDEX idx_word_phrase_en_es_word_translation_dst_lang
ON word_phrase_en_es (word_translation, dst_lang);


-- create table for each src_lang
CREATE TABLE banned_word_en (
    word TEXT PRIMARY KEY,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    reason TEXT NOT NULL,
    attempts SMALLINT NOT NULL
);
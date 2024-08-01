-- create table for each src_lang
CREATE TABLE word_phrase_en (
    word TEXT,
    dst_lang CHAR(2),
    phrase TEXT,
    word_translation TEXT,
    phrase_translation TEXT,
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
CREATE TABLE error_word_en (
    word TEXT,
    dst_lang CHAR(2),
    error_count SMALLINT
) PARTITION BY LIST (dst_lang);

-- create partition table for each dst_lang
CREATE TABLE error_word_en_es PARTITION OF error_word_en
FOR VALUES IN ('es');

-- create index (word, dst_lang) for each dst_lang
CREATE INDEX idx_error_word_en_es_word_dst_lang
ON error_word_en_es (word, dst_lang);

-- create index (word_translation, dst_lang) for each dst_lang
CREATE INDEX idx_error_word_en_es_word_translation_dst_lang
ON error_word_en_es (word_translation, dst_lang);
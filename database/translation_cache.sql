CREATE TABLE word_phrase_pair (
    src_lang CHAR(2) NOT NULL,
    dst_lang CHAR(2) NOT NULL,
    src_word TEXT NOT NULL,
    dst_word TEXT NOT NULL,
    src_phrase TEXT NOT NULL,
    dst_phrase TEXT NOT NULL,
    PRIMARY KEY (src_lang, dst_lang, src_word, dst_word)
);

CREATE TABLE problematic_word (
    src_lang CHAR(2) NOT NULL,
    dst_lang CHAR(2) NOT NULL,
    src_word TEXT NOT NULL,
    problem_count INT NOT NULL DEFAULT 0,
    PRIMARY KEY (src_lang, dst_lang, src_word)
);
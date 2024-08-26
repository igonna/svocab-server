
-context: none
create_account_no_email (
    device_id
    lang_src
    lang_dst
    basic_topic_coeff OR None
) -> refresh_token,user_id

-context: none
attach_email_to_account (
    email/google_token
) -> success_result

create_account_with_email (
    email,
    device_id
) -> refresh_token,user_id


-context: another account without email, but sign in existing accoutn
sign_in_to_account_with_email (
    email,
    device_id
) -> refresh_token, user_id


register_code_request-email@gmail.com
register_attempts-email@gmail.com
register_code-email@gmail.com

sign_in_code-email@gmail.com
attach_code-email@gmail.com
register_attempts*

user_auth

user_account



REGISTER USER DATABASE WORK

CREATE ROW
users
user_sequence
user_state
user_language_pairs -> user_words
user_devices
user_statistics
(optional)user_custom_global_topic_options



DELETE USER DATABASE WORK



REGISTER

1. register -> refresh_token

# 1.register_without_email -> refresh_token
#     lang_src
#     lang_dst
#     skip_value(optional)
#     topic_id(optional)


# 2.get_code_for_email_register -> code
#     email

# 3.register_email -> refresh_token
#     code
#     email


# 4.register_google -> refresh_token
#     google_token



SIGNIN

5.get_code_for_email_sign_in -> code
    email

6.sign_in_to_acccount_email -> refresh_token
    code
    email

7.sign_in_to_account_google -> refresh_token
    google_token


LINK

8.get_code_for_email_link -> code
    email

9.link_email_to_account -> ok
    code
    email

10.link_google_to_account -> ok
    google_token


CHANGE

----change_e;verify
11.get_code_for_change_email -> code
    email

12.verify_change_email -> email_changed
    code


DELETE

13.delete_account -> ok
    refresh_token


ACCOUNT INFO

14.get_email_and_plus_info -> email, has_plus

STATE INFO

15.get_state -> state

STATISTICS

16.get_statistics -> statistics

PROGRESS WORDS

17.get_learned_words -> list

18.remove_learned_word -> ok
    src_lang
    word

19.get_hidden_words -> list

20.remove_hidden_word -> ok
    src_lang
    word

USER LANGUAGES

21.get_languages_with_topics -> JSON langs:topics (topic{type,title,enabled,})

22.set_language_pair_mix_value -> ok
    src_lang
    dst_lang
    float

23.set_language_pair_enabled -> ok
    src_lang
    dst_lang
    boolean


TOPIC OPTIONS

24.set_topic_enabled -> ok
    topic_id
    boolean

25.set_topic_skip_value -> ok
    topic_id
    float

26.set_topic_mix_value -> ok
    topic_id
    float

27.set_topics_positions -> ok
    topic_id_1=0
    topic_id_2=3

28.get_topic_words -> list
    topic_id

29.create_topic -> topic_id
    (lang_pair_id)
    src_lang
    dst_lang

30.delete_topic -> ok
    topic_id

31.set_word_count_type -> ok
    topic_id
    int

32.set_word_order_type -> ok
    topic_id
    int

33.set_topic_filter_option -> ok
    topic_id
    id
    boolean

34.set_topic_title -> ok
    topic_id
    title

35.get_topic_sources_title -> json
    topic_id

36.delete_topic_source -> ok
    topic_id
    source_type
    source_id


37.delete_topic_failed_source -> ok
    topic_id
    source_type
    source_id

38.add_topic_source -> ok
    topic_id
    source_type
    content

39.add_topic_document_source -> ok
    topic_id
    document

40.update_text_source -> ok
    topic_id
    source_id
    content


40.1.get_topic_content -> string-content
    topic_id

40.2.get_basic_topic -> string-content, topic_id


DEVICES INFO

41.get_devices -> list

42.revoke_device -> ok
    device_num


FOCUS GROUP

43.get_focus_group -> list

44.complete_focus_group -> new_focus_group
    focus_group_id

45.update_focus_group_hidden_word -> new_focus_group
    focus_group_id
    word_id

46.update_focus_group -> ok/new_focus_group_ready_status

TOKEN

47.get_bare_token -> bare_token
    device_num
    user_id
    refresh_token
    device_os_version
    device_model

LANGAUGES

48.get_supported_languages -> list




get_topic_sources -> json_titles:texts;documents;website_urls;youtube_urls

titles;failed_titles;ids

topics_basic {
    title
    type
    position
    enabled
}

topic_advanced {
    mix_value
    skip_value
}

topic_gen_advanced {
    mix_value
    skip_value
    order_type
}

topic: {
    type: 0 // 1 = gen, 0 = glob
    title:hello_world
    position:0
    (1)order: 0 // 0 = populrity, 1 = appearance
    skip_value: 0.1
    mix_value: 0.5
}




create_new_topic -> topic_id

add_new_topic_source -> ok/error
add_new_topic_file -> ok/error
update_topic_source -> ok/error
delete_topic_source -> ok/error

change_topic_options -> ok/error
change_topic_title -> ok/error



state:
subscriber = 0
learned_this_time
check_points = 16
focus_group = en;ru;hello;16;en;ru;hi;9;

revoked=1;2;3 // only on server


----------state 2.0:
seq_id=0
learned_this_time=9012
check_points=16
focus_group=190;16; // word_id;state;


lang_src;lang_dst;word;word_status;word_pointer;

word_pointer;word_status


sequence_id required to update focus_group on change of sequence_id
IMPORTANT NOTE:
focus group is bound to sequence_id
if sequence generated and sequence_id updated, focus group must be cleaned


focus group example:
hello 0
hi 0
wow 1
what 1

we already have 2 words in focus group,
but our focus size is 4,
so we need to fetch new words len = focus_size - words_left_in_focus_group_size
from last learned word

new focus group:

hello 20
hi 0
good 1
well 2



page 15,16,17

focus_group

word;phase_num;phase_1_error;phase_2_error;page_pointer;word_pointer;

word:1;1;0;1950;20



update_word:

hello;16;
hello = word

word_phases:
closed
first_phase_opened
second_phase_opened
third_phase_opened

16 is:
0 bit and 1 bit = cur_phase_num
2 bit = phase_0_error
3 bit = phase_1_error
4 bit = phase_2_error
5 bit = hidden

if phase != 0 for other devices phrase generation must be free



get_focus_group

read db user_generate_sequence
read db focus_size
read db least_not_learned_word_pointer

but how to skip learned words after pointer ?

after least_not_learned_word_pointer could be learned words

sequence must be generated when topic changed
sequence must be generated when repeat required
sequence must be generated when hidden word



generated sequence:

word;lang;states






on_hidden_word:
    get_focus_group_with_hidden_word:
        update_sequence_pointer
        modify_state

on_phase_update:
    just_update_state




on_last_word_in_focus_group_update:
    show_local_focus_group_result
    complete_focus_group -> saves_learned_words_on_server_side -> generates_new_focus_group


get_focus_group:
    check_if_topics_updated -> means_new_generation_required
    check_if_repeat_time_reached -> means_new_generation_required




get_focus_group (expanded):
    take focus_size words from last_taken_page_pointer + last_taken_word_pointer - focus_size

    but what if pointer were for focus_size = 3 but now focus size decreased to 2


hello
hi
how
what
who
where
why - pointer
---
buy
world
cup


if focus_size not changed no problem
hello
hi
how
            who
what
where
why
buy  - pointer
---
world
cup

if focus_size changed to 2 there is a problem

hello
hi
how
            who
what
where
------
why
buy  - pointer
------
world
cup





hello
hi
how
what
who
where
why - pointer
---
buy
world
cup


if focus_size not changed no problem
hello
hi
how
            who
what
where
why
buy  - pointer
---
world
cup

if focus_size changed to 2 there is a problem

hello
hi
how
            who
what
where
------
why
buy  - pointer
------
world
cup


least_seq_in_focus_pointer
last_seq_in_focus_pointer

seq_pointer_last could shrink to seq_least_pointer and we will 
lose information about words that were already learned or hidden, 
and on next future expand they will be again in the focus_group which is wrong



how to mix learned and to_repeat



not_learned
to_repeat


50% not_learned
50% to_repeat




hello----lowest_not_learned
hi - learned
what - not_learned
wow - learned
where----highest_learned


hello----lowest_not_learned
hi - learned ----lowest_learned
what - not_learned
wow ----highest_learned
where - not learned


hello----highest_learned
hi----lowest_not_learned
what
wow
where


how much to fetch?
lowest_not_learned+between_pointer_count
highest_learned - lowest_not_learned


how many to fetch?
lowest_not_learned_pointer + focus_size + count_learned_between_pointers




every Pointer which is bigger than lowest_not_learned must be removed


if (pointer <= lowest_not_learned_pointer)
    remove(pointer)



// fetch only not null words
SELECT 
    ARRAY(
        SELECT unnest(data_array) 
        FROM example_table
        WHERE id = 1  -- specify the row you want to operate on
        AND unnest IS NOT NULL 
        AND unnest <> '' 
        LIMIT 10
    ) AS filtered_array
FROM example_table
WHERE id = 1;

LIMIT = focus_size + len(brought_back_words)



// set array values
UPDATE sal_emp SET words[4] = NULL, words[6] = NULL
    WHERE user_id = 1829;




-- user_score BIGINT, -- total_learned * average_per_day_learned
#REGISTER
(+)1.register -> refresh_token

#LINK
(+)2.get_code_for_email_link -> code
    email

(+)3.link_email_to_account -> ok
    code
    email

(+)4.link_google_to_account -> ok
    google_token

#SIGNIN
(+)5.get_code_for_email_sign_in -> code
    email

(+)6.sign_in_to_acccount_email -> refresh_token
    code
    email

(+)7.sign_in_to_account_google -> refresh_token
    google_token

#ACCOUNT INFO
(+)8.get_info -> email, has_plus

#DELETE
(+)9.delete_account -> ok
    refresh_token

#USER DEVICES
(+)10.get_devices -> list

(+)11.revoke_device -> ok
    device_num

(+)12.get_bare_token -> bare_token
    refresh_token
    device_os_version
    device_model

#LANGAUGES
(+)13.get_supported_languages -> list

#USER LANGUAGES
(+)14.set_language_pair_enabled -> ok
    src_lang
    dst_lang
    boolean

(+)15.set_language_pair_mix_value -> ok
    src_lang
    dst_lang
    float

(***)16.get_language_pair_mix_options -> json
    src_lang
    dst_lang

#USER TOPICS
(+)17.create_topic -> json
    src_lang
    dst_lang

(+)18.delete_topic -> ok
    topic_id

(+)19.get_topic_generation_window -> json
    topic_id

(+)20.set_topic_title -> ok
    topic_id
    title

(+)21.add_topic_source -> ok
    topic_id
    source_type
    body:content

(+)22.get_topic_source_text_content -> body
    topic_id
    text_id

(+)23.set_topic_source_text -> ok
    topic_id
    source_id
    body:content

(+)24.add_topic_source_document -> ok
    topic_id
    document

(+)25.set_topic_option -> ok
    topic_id
    topic_option_name
    value

(+)26.set_topic_enabled -> ok
    topic_id
    boolean

(+)26.set_user_global_topic_enabled -> ok
    topic_id
    dst_lang
    boolean

(+)27.set_topic_position -> ok
    topic_id

(+)28.get_topic_words -> list
    topic_id

(+)29.delete_topic_source -> ok
    topic_id
    source_type
    source_id

(+)30.get_user_topics -> json

(+)31.get_topic_mix_options -> json
    topic_id

(+)32.get_topic_skip_options -> json
    topic_id

(+)33.get_first_user_global_topic_id -> json/not_found

#GLOBAL TOPICS
(+)36.add_global_topic_with_user_topic -> ok
    user_topic_id
    popularity

(+)37.update_global_topic_with_user_topic -> ok
    user_topic_id
    global_topic_id

(+)37.delete_global_topic -> ok
    global_topic_id

#FOCUS GROUP
(***)39.get_focus_group -> json

(***)44.get_previous_focus_group_result -> json

(***)42.get_focus_group_with_hidden_word -> json
    word_id

(***)40.get_focus_group_last_update_time -> int

(***)41.set_word_phase_passed -> ok
    word_id
    phase_num
    mistake:bool

(***)43.complete_focus_group -> json


(***)45.set_offer_item_passed -> ok
    type

#USER CHECKPOINTS
(+)45.get_checkpoints -> json

(+)46.set_checkpoint -> ok
    checkpoint_name
    value

#USER STATISTICS
(+)47.get_statistics -> json

#PROGRESS WORDS
(+)48.get_learned_words -> list

(+)49.remove_learned_word -> ok
    src_lang
    body:word

(+)50.get_hidden_words -> list

(+)51.remove_hidden_word -> ok
    src_lang
    body:word

#USER SUBSCRIPTION
(***)52.add_subscription -> ok
    purchase_token
    # just send purchase token to google api
    # get response and set expiration time

(***)53.update_subscription_status -> ok
    # can update subscription status only if subscription exists

(***)54.is_subscription_cancelled -> json





#POSTGRES
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

#REDIS
last_devices_models_update
server_secret_key
user_state
last_global_rank_update

#USER WORDS
word;confirmation_level;hidden;last_update_time
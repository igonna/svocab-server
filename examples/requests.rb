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
(+)10.get_bare_token -> bare_token
    refresh_token
    device_os_version
    device_model

(+)11.log_out_of_all_devices -> ok

#OFFERS
(+)11.set_offer_passed -> ok
    offer_type

#LANGAUGES
(+)12.get_supported_languages -> list

(+)13.set_language_pair_enabled -> ok
    src_lang
    dst_lang
    boolean

#USER TOPICS
(+)14.create_topic() -> ok
    src_lang
    dst_lang
    origin: https:// (optional for links)
    type: prompt, link, text, youtube, html, file
    body: multipart/form-data: file

prompt:
    type: prompt
    origin: no header
    body: all the prompt

link (try to extract as html, if failed, use this as link):
    type: link
    origin: no header
    body: htpps://...

text:
    type: text
    origin: no header
    body: all the text

youtube:
    type: youtube
    origin: https://youtube.com/...
    body: title + \n + captions

html:
    type: html
    origin: https://...
    body: all the html

file:
    type: file
    origin: no header
    body: empty
    multipart/from-data: all the file

(+)15.get_topics -> json

(+)16.get_topic_progress -> json
    topic_id

(+)17.rename_topic -> ok
    topic_id
    body: new_name

(+)18.edit_topic -> ok
    topic_id
    body: new_content

(+)19.delete_topic -> ok
    topic_id

(+)20.get_document_support_info -> json

#SEQUENCE ITEMS
(-)21.get_current_item -> json
(-)22.get_next_item -> json
    body_json: {item_id, 
        action_type: answers_sum || hide || rate_us, 
        value (arr for answers_sum, true || false for rate_us, empty for hide)
    }

(+)21.is_updated -> ok

(-)22.get_item -> json
    body_json:

request_types: [item_passed, hide]
item_types: []

body_json:
    request_type:
    item_type:


#HIDDEN WORDS
(+)24.get_hidden_words -> json

(+)25.remove_hidden_word -> ok
    src_lang
    body:word

#USER SUBSCRIPTION
(-)26.add_subscription -> ok
    purchase_token
    # just send purchase token to google api
    # get response and set expiration time

(-)27.update_subscription_status -> ok
    # can update subscription status only if subscription exists

(-)28.is_subscription_cancelled -> json





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
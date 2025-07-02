#include "AppleId.h"
#include <drogon/HttpClient.h>
#include "utils/exceptions.h"
#include "utils/utils.h"
#include <trantor/utils/Date.h>
#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include "core/website_fetch/website_fetch.h"

using namespace drogon;

void AppleId::initAndStart(const Json::Value &config) {
    apple_bundle_id = config["bundle_id"].asString();
}

Task<Json::Value> fetch_apple_id_public_keys() {
    const std::string apple_id_public_keys_redis_key = "apple_id_public_keys";
    const uint32_t apple_id_public_keys_cache_seconds = 86400;

    auto rc = app().getRedisClient();
    auto result = co_await rc->execCommandCoro("get %s", apple_id_public_keys_redis_key.c_str());

    std::string public_keys;
    if (result.isNil()) {
        public_keys = co_await core::website_fetch::fetch_website_coro("https://appleid.apple.com/auth/keys");

        co_await rc->execCommandCoro("SET %s %s EX %d",
            apple_id_public_keys_redis_key.c_str(),
            public_keys.c_str(),
            apple_id_public_keys_cache_seconds
        );
    } else {
        public_keys = result.asString();
    }

    Json::Value keys_root;
    if (!Json::Reader().parse(public_keys, keys_root)) {
        throw std::runtime_error("json_parse_error");
    }

    if (!keys_root.isMember("keys")) throw std::runtime_error("apple id public keys not contain keys field");

    co_return keys_root;
}

std::vector<uint8_t> base64url_decode(const std::string &input) {
    std::string b64 = input;
    std::replace(b64.begin(), b64.end(), '-', '+');
    std::replace(b64.begin(), b64.end(), '_', '/');
    while (b64.size() % 4 != 0) b64 += '=';

    BIO *bio, *b64bio;
    bio = BIO_new_mem_buf((void *)b64.data(), b64.size());
    b64bio = BIO_new(BIO_f_base64());
    bio = BIO_push(b64bio, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    std::vector<uint8_t> output(b64.size());
    int decodedLength = BIO_read(bio, output.data(), b64.size());
    output.resize(decodedLength);

    BIO_free_all(bio);
    return output;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

std::string convertRSAKeyToPEM(const std::string &n_b64url, const std::string &e_b64url) {
    auto n_bin = base64url_decode(n_b64url);
    auto e_bin = base64url_decode(e_b64url);

    BIGNUM *n = BN_bin2bn(n_bin.data(), n_bin.size(), NULL);
    BIGNUM *e = BN_bin2bn(e_bin.data(), e_bin.size(), NULL);

    RSA *rsa = RSA_new();
    RSA_set0_key(rsa, n, e, NULL);

    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_RSA_PUBKEY(bio, rsa);

    BUF_MEM *buf;
    BIO_get_mem_ptr(bio, &buf);

    std::string pem(buf->data, buf->length);

    BIO_free_all(bio);
    RSA_free(rsa);

    return pem;
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif


Task<std::tuple<std::string, std::optional<std::string>>> AppleId::get_email_from_apple_id_token(const std::string& apple_id_token) try {
    auto apple_id_public_keys = co_await fetch_apple_id_public_keys();

    auto decoded = jwt::decode(apple_id_token);
    auto kid = decoded.get_header_claim("kid").as_string();

    const auto& keys = apple_id_public_keys["keys"];
    for (const auto& key : keys) {
        if (key["kid"].asString() != kid) continue;

        auto n_b64 = key["n"].asString();
        auto e_b64 = key["e"].asString();

        auto pem = convertRSAKeyToPEM(n_b64, e_b64);

        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::rs256(pem, "", "", ""))
            .with_issuer("https://appleid.apple.com")
            .with_audience(apple_bundle_id);

        try {
            verifier.verify(decoded);

            std::string user_id = decoded.has_payload_claim("sub") ? decoded.get_payload_claim("sub").as_string() : "";
            std::optional<std::string> email = std::nullopt;
            
            if (decoded.has_payload_claim("email")) {
                auto email_tmp = decoded.get_payload_claim("email").as_string();
                if (email_tmp.size()) email = email_tmp;
            }

            co_return {user_id, email};
        } catch (const std::exception& e) {
            // continue
        }
    }

    throw std::runtime_error("not valid apple id token");
} catch (const std::exception& e) {
    throw;
}
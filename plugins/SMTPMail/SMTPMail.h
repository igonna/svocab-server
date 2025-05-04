#pragma once

#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>

class SMTPMail : public drogon::Plugin<SMTPMail>
{
public:
    SMTPMail() = default;
    void initAndStart(const Json::Value &config) override;
    void shutdown() override;

    /** Send an email
     * return : An ID of the email.
     */
    std::string sendEmail(
        const std::string &mailServer,
        const uint16_t &port,
        const std::string &from,
        const std::string &to,
        const std::string &subject,
        const std::string &content,
        const std::string &user,
        const std::string &passwd,
        bool isHTML,
        const std::function<void(const std::string &)> &cb = {}
    );
};
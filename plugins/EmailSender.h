#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>

class EmailSender : public drogon::Plugin<EmailSender>
{
public:
	EmailSender() {}
	void initAndStart(const Json::Value &config) override;
	void shutdown() override;

	bool is_valid_email(const std::string &email);

	std::string generate_email_code();

	void send_email(const std::string& email, const std::string& code);

	drogon::Task<std::string> get_email_from_google_token(const std::string &token);

	std::string get_email_type(const std::string& email);

public:
	// exceptions
	struct google_token_not_valid_exception : public std::exception {
		const char* what() const noexcept override {
			return "invalid_token";
		}
	};

private:
    std::string smtp_server;
    std::string smtp_user;
    std::string smtp_password;
};
#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>
#include <unordered_set>

struct BannedWordsList {
	BannedWordsList(uint32_t last_added_time, std::unordered_set<std::string>&& words) : last_added_time(last_added_time), words(std::move(words)) {}
	BannedWordsList(BannedWordsList&& other) : last_added_time(other.last_added_time), words(std::move(other.words)) {}
	BannedWordsList() {}
	
	uint32_t last_added_time = 0;
	std::unordered_set<std::string> words;
};

class CurrentBannedWords {
public:
	CurrentBannedWords(std::shared_ptr<BannedWordsList> shared_words, std::unordered_set<std::string>&& current_words) 
		: shared_words(shared_words), current_words(std::move(current_words)) {}

	CurrentBannedWords(CurrentBannedWords&& other) : shared_words(other.shared_words), current_words(std::move(other.current_words)) {}

	CurrentBannedWords& operator=(CurrentBannedWords&& other) {
		shared_words = other.shared_words;
		current_words = std::move(other.current_words);
		return *this;
	}

	inline bool is_banned(const std::string& word) {
		return shared_words->words.contains(word) || current_words.contains(word);
	}

private:
	std::unordered_set<std::string> current_words;
	std::shared_ptr<BannedWordsList> shared_words;
};

class BannedWords : public drogon::Plugin<BannedWords> {
public:
	BannedWords();
	void initAndStart(const Json::Value &config) override;
	void shutdown() override;

	drogon::Task<CurrentBannedWords> get_current_banned_words_for_src_lang(const std::string& src_lang);

private:
	std::unordered_map<std::string, std::atomic<std::shared_ptr<BannedWordsList>>> banned_words_by_lang;
};
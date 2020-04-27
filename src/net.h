#pragma once

#include "utility.h"

namespace builder
{

class url
{
public:
	url(const char* p);
	url(std::string s={});

	const char* c_str() const;
	const std::string& string() const;

	std::string file() const;

private:
	std::string s_;
};


class curl_downloader
{
public:
	curl_downloader(fs::path where, url u);

	void start();
	void join();
	void interrupt();

	fs::path file() const;

private:
	fs::path where_;
	url url_;
	fs::path path_;
	std::FILE* file_;
	std::thread thread_;
	std::atomic<bool> interrupt_;
	std::optional<bailed> bailed_;

	void run();

	static size_t on_write_static(
		char* ptr, size_t size, size_t nmemb, void* user) noexcept;

	void on_write(char* ptr, std::size_t n) noexcept;
};

}	// namespace

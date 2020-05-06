#pragma once

#include "utility.h"

namespace mob
{

class context;

struct curl_init
{
	curl_init();
	~curl_init();

	curl_init(const curl_init&) = delete;
	curl_init& operator=(const curl_init&) = delete;
};


class url
{
public:
	url(const char* p);
	url(std::string s={});

	const char* c_str() const;
	const std::string& string() const;
	bool empty() const;

	std::string filename() const;

private:
	std::string s_;
};


class curl_downloader
{
public:
	curl_downloader(const context* cx=nullptr);

	void start(const url& u, const fs::path& file);
	void join();
	void interrupt();
	bool ok() const;

private:
	const context& cx_;
	url url_;
	fs::path path_;
	file_ptr file_;
	std::thread thread_;
	std::size_t bytes_;
	std::atomic<bool> interrupt_;
	bool ok_;

	void run();

	static size_t on_write_static(
		char* ptr, size_t size, size_t nmemb, void* user) noexcept;

	void on_write(char* ptr, std::size_t n) noexcept;


	static int on_debug_static(
		CURL* handle, curl_infotype type,
		char* data, size_t size, void *user) noexcept;

	void on_debug(curl_infotype type, std::string_view s);
};

}	// namespace

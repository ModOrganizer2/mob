#pragma once

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


class downloader
{
public:
	downloader(fs::path where, url u);

	void start();
	void join();

	fs::path file() const;

private:
	fs::path where_;
	url url_;
	fs::path path_;
	std::FILE* file_;
	std::thread thread_;
	bool bailed_;

	void run();

	static size_t on_write_static(
		char* ptr, size_t size, size_t nmemb, void* user) noexcept;

	void on_write(char* ptr, std::size_t n) noexcept;
};


fs::path download(const url& u);

}	// namespace

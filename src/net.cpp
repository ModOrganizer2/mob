#include "pch.h"
#include "net.h"
#include "conf.h"
#include "op.h"
#include "utility.h"

namespace builder
{

std::string redir_nul()
{
	if (conf::verbose())
		return {};
	else
		return " > NUL";
}


url::url(const char* p)
	: s_(p)
{
}

url::url(std::string s)
	: s_(std::move(s))
{
}

const char* url::c_str() const
{
	return s_.c_str();
}

const std::string& url::string() const
{
	return s_;
}

std::string url::file() const
{
	const auto pos = s_.find_last_of("/");

	if (pos == std::string::npos)
		return s_;
	else
		return s_.substr(pos + 1);
}


curl_downloader::curl_downloader()
	: interrupt_(false), ok_(false)
{
}

fs::path curl_downloader::path_for_url(const fs::path& where, const url& u)
{
	return where / u.file();
}

void curl_downloader::start(const fs::path& where, const url& u)
{
	where_ = where;
	url_ = u;
	path_ = path_for_url(where, u);
	ok_ = false;

	debug("downloading " + url_.string() + " to " + path_.string());

	if (conf::dry())
		return;

	thread_ = std::thread([&] { run(); });
}

void curl_downloader::join()
{
	if (thread_.joinable())
		thread_.join();
}

void curl_downloader::interrupt()
{
	interrupt_ = true;
}

bool curl_downloader::ok() const
{
	return ok_;
}

fs::path curl_downloader::file() const
{
	return path_;
}

void curl_downloader::run()
{
	auto* c = curl_easy_init();

	curl_easy_setopt(c, CURLOPT_URL, url_.c_str());
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_write_static);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1l);

	file_deleter output_deleter(path_);

	const auto r = curl_easy_perform(c);
	file_.reset();

	if (interrupt_)
		return;

	if (r == 0)
	{
		long h = 0;
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &h);

		if (h == 200)
		{
			ok_ = true;
			output_deleter.cancel();
		}
		else
		{
			error(url_.string() + " http code: " + std::to_string(h));
		}
	}
	else
	{
		error(url_.string() + " curl: " + curl_easy_strerror(r));
	}

	curl_easy_cleanup(c);
}

size_t curl_downloader::on_write_static(
	char* ptr, size_t size, size_t nmemb, void* user) noexcept
{
	auto* self = static_cast<curl_downloader*>(user);

	if (self->interrupt_)
	{
		debug("downloader: interrupting");
		return (size * nmemb) + 1; // force failure
	}

	self->on_write(ptr, size * nmemb);
	return size * nmemb;
}

void curl_downloader::on_write(char* ptr, std::size_t n) noexcept
{
	if (!file_)
	{
		op::create_directories(where_);
		file_ .reset(_wfopen(path_.native().c_str(), L"wb"));
	}

	std::fwrite(ptr, n, 1, file_.get());
}

}	// namespace

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
		return "> NUL";
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


downloader::downloader(fs::path where, url u) :
	where_(std::move(where)), url_(std::move(u)),
	file_(nullptr), bailed_(false)
{
	path_ = where_ / url_.file();
}

void downloader::start()
{
	debug("downloading " + url_.string());

	op::create_directories(where_);

	if (fs::exists(path_))
	{
		debug("download " + path_.string() + " already exists");
		return;
	}

	thread_ = std::thread([&]
		{
			try
			{
				run();
			}
			catch(bailed)
			{
				bailed_ =true;
			}
		});
}

void downloader::join()
{
	if (thread_.joinable())
		thread_.join();

	if (bailed_)
		throw bailed();
}

fs::path downloader::file() const
{
	return path_;
}

void downloader::run()
{
	auto* c = curl_easy_init();

	curl_easy_setopt(c, CURLOPT_URL, url_.c_str());
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_write_static);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1l);


	const auto r = curl_easy_perform(c);

	if (r == 0)
	{
		long h = 0;
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &h);

		if (h != 200)
			bail_out(url_.string() + " http code: " + std::to_string(h));
	}
	else
	{
		bail_out(url_.string() + " curl: " + curl_easy_strerror(r));
	}

	curl_easy_cleanup(c);

	if (file_)
		std::fclose(file_);
}

size_t downloader::on_write_static(
	char* ptr, size_t size, size_t nmemb, void* user) noexcept
{
	auto* self = static_cast<downloader*>(user);
	self->on_write(ptr, size * nmemb);
	return size * nmemb;
}

void downloader::on_write(char* ptr, std::size_t n) noexcept
{
	if (!file_)
		file_ = _wfopen(path_.native().c_str(), L"wb");

	std::fwrite(ptr, n, 1, file_);
}


fs::path download(const url& u)
{
	downloader d(paths::cache(), u);

	d.start();
	d.join();

	return d.file();
}

}	// namespace

#include "pch.h"
#include "net.h"
#include "conf.h"
#include "op.h"
#include "utility.h"
#include "context.h"

namespace mob
{

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

bool url::empty() const
{
	return s_.empty();
}

std::string url::filename() const
{
	const auto pos = s_.find_last_of("/");

	if (pos == std::string::npos)
		return s_;
	else
		return s_.substr(pos + 1);
}


curl_downloader::curl_downloader(const context* cx)
	: cx_(cx), bytes_(0), interrupt_(false), ok_(false)
{
	if (!cx_)
		cx_ = context::dummy();
}

void curl_downloader::start(const url& u, const fs::path& path)
{
	url_ = u;
	path_ = path;
	ok_ = false;

	cx_->log(
		context::net,
		"downloading " + url_.string() + " to " + path_.string());

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
	cx_->log(context::interrupted, "will interrupt curl");
	interrupt_ = true;
}

bool curl_downloader::ok() const
{
	return ok_;
}

void curl_downloader::run()
{
	cx_->log(context::net_trace, "curl: initializing " + url_.string());

	auto* c = curl_easy_init();
	guard g([&]{ curl_easy_cleanup(c); });

	char error_buffer[CURL_ERROR_SIZE + 1] = {};

	curl_easy_setopt(c, CURLOPT_URL, url_.c_str());
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_write_static);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1l);
	curl_easy_setopt(c, CURLOPT_ERRORBUFFER, error_buffer);

	if (context::enabled(context::net_dump))
	{
		curl_easy_setopt(c, CURLOPT_DEBUGFUNCTION, on_debug_static);
		curl_easy_setopt(c, CURLOPT_DEBUGDATA, this);
		curl_easy_setopt(c, CURLOPT_VERBOSE, 1l);
	}

	file_deleter output_deleter(path_);

	cx_->log(context::net_trace, "curl: performing " + url_.string());
	const auto r = curl_easy_perform(c);
	cx_->log(context::net_trace, "curl: transfer finished " + url_.string());

	file_.reset();

	if (interrupt_)
	{
		cx_->log(context::net_trace, "curl: " + url_.string() + " interrupted");
		return;
	}

	if (r == CURLE_OK)
	{
		long h = 0;
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &h);

		if (h == 200)
		{
			cx_->log(context::net_trace, "curl: http 200 " + url_.string());
			cx_->log(context::net_trace, "curl: got " + std::to_string(bytes_) + " bytes");

			ok_ = true;
			output_deleter.cancel();
		}
		else
		{
			cx_->log(
				context::error,
				"curl: http " + std::to_string(h) + " " + url_.string());
		}
	}
	else
	{
		cx_->log(
			context::error,
			std::string("curl: ") +
			curl_easy_strerror(r) + ", " + error_buffer + ", " +
			url_.string());
	}
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
		op::create_directories(path_.parent_path(), cx_);

		cx_->log(context::net_trace, "opening " + path_.string());
		file_.reset(_wfopen(path_.native().c_str(), L"wb"));
	}

	bytes_ += n;
	std::fwrite(ptr, n, 1, file_.get());
}

int curl_downloader::on_debug_static(
	CURL*, curl_infotype type,
	char* data, size_t size, void *user) noexcept
{
	auto* self = static_cast<curl_downloader*>(user);
	self->on_debug(type, {data, size});
	return 0;
}

bool a_bit_too_much(std::string_view s)
{
	static const char* strings[] =
	{
		"schannel: encrypted data",
		"schannel: encrypted cached",
		"schannel: decrypted data",
		"schannel: decrypted cached",
		"schannel: client wants",
		"schannel: failed to decrypt data",
		"schannel: schannel_recv",
		"schannel: Curl_read_plain"
	};

	for (std::size_t i=0; i<std::extent_v<decltype(strings)>; ++i)
	{
		if (s.starts_with(strings[i]))
			return true;
	}

	return false;
}

void curl_downloader::on_debug(curl_infotype type, std::string_view s)
{
	// try to minimize allocations
	static thread_local std::string buffer(500, ' ');

	auto do_log = [&](std::string_view what, std::string_view s)
	{
		const char* start = s.data();
		const char* end = s.data() + s.size();
		const char* p = start;

		for (;;)
		{
			if (p == end || *p == '\n' || *p == '\r')
			{
				if (p != start)
				{
					if (what.empty())
					{
						buffer = "curl: ";
						buffer.append(start, p);
					}
					else
					{
						buffer = "curl: ";
						buffer.append(what);
						buffer.append(": ");
						buffer.append(start, p);
					}

					cx_->log(context::net_dump, buffer);
				}

				if (p == end)
					break;

				while (p != end && (*p == '\n' || *p == '\r'))
					++p;

				start = p;
			}
			else
			{
				++p;
			}
		}
	};


	switch (type)
	{
		case CURLINFO_TEXT:
			if (!a_bit_too_much(s))
				do_log("", s);

			break;

		case CURLINFO_HEADER_IN:
			do_log("header in", s);
			break;

		case CURLINFO_HEADER_OUT:
			do_log("header out", s);
			break;

		case CURLINFO_DATA_IN:
			//do_log("data in", s);
			break;

		case CURLINFO_DATA_OUT:
			//do_log("data out", s);
			break;

		case CURLINFO_SSL_DATA_IN:
			//do_log("ssl data in", s);
			break;

		case CURLINFO_SSL_DATA_OUT:
			//do_log("ssl data out", s);
			break;

		case CURLINFO_END:
		default:
			break;
	}
}

}	// namespace

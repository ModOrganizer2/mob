#include "pch.h"
#include "net.h"
#include "conf.h"
#include "op.h"
#include "utility.h"
#include "context.h"

namespace mob
{

curl_init::curl_init()
{
	curl_global_init(CURL_GLOBAL_ALL );
}

curl_init::~curl_init()
{
	curl_global_cleanup();
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

bool url::empty() const
{
	return s_.empty();
}

std::string url::filename() const
{
	std::string path;

	{
		auto* h = curl_url();
		guard g([&]{ curl_url_cleanup(h); });

		auto r = curl_url_set(h, CURLUPART_URL, s_.c_str(), 0);

		if (r != CURLUE_OK)
			gcx().bail_out(context::net, "bad url '" + s_ + "'");

		char* buffer = nullptr;
		r = curl_url_get(h, CURLUPART_PATH, &buffer, 0);

		if (r != CURLUE_OK)
			gcx().bail_out(context::net, "bad url '" + s_ + "'");

		guard g2([&]{ curl_free(buffer); });

		path = buffer;
	}

	const auto pos = path.find_last_of("/");

	if (pos == std::string::npos)
		return path;
	else
		return path.substr(pos + 1);
}


curl_downloader::curl_downloader(const context* cx)
	: cx_(cx ? *cx : gcx()), bytes_(0), interrupt_(false), ok_(false)
{
}

void curl_downloader::start(const url& u, const fs::path& path)
{
	url_ = u;
	path_ = path;
	ok_ = false;

	cx_.debug(context::net,
		"downloading " + url_.string() + " to " + path_.string());

	if (conf::dry())
		return;

	thread_ = std::thread([&]
	{
		try
		{
			run();
		}
		catch(...)
		{
			// eat it
		}
	});
}

void curl_downloader::join()
{
	if (thread_.joinable())
		thread_.join();
}

void curl_downloader::interrupt()
{
	cx_.debug(context::interruption, "will interrupt curl");
	interrupt_ = true;
}

bool curl_downloader::ok() const
{
	return ok_;
}

void curl_downloader::run()
{
	cx_.trace(context::net, "curl: initializing " + url_.string());

	auto* c = curl_easy_init();
	guard g([&]{ curl_easy_cleanup(c); });

	char error_buffer[CURL_ERROR_SIZE + 1] = {};

	curl_easy_setopt(c, CURLOPT_URL, url_.c_str());
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_write_static);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(c, CURLOPT_PROGRESSFUNCTION, on_progress_static);
	curl_easy_setopt(c, CURLOPT_PROGRESSDATA, this);
	curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, on_xfer_static);
	curl_easy_setopt(c, CURLOPT_XFERINFODATA, this);
	curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0l);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1l);
	curl_easy_setopt(c, CURLOPT_ERRORBUFFER, error_buffer);

	if (conf::log_dump())
	{
		curl_easy_setopt(c, CURLOPT_DEBUGFUNCTION, on_debug_static);
		curl_easy_setopt(c, CURLOPT_DEBUGDATA, this);
		curl_easy_setopt(c, CURLOPT_VERBOSE, 1l);
	}

	file_deleter output_deleter(cx_, path_);

	cx_.trace(context::net, "curl: performing " + url_.string());
	const auto r = curl_easy_perform(c);
	cx_.trace(context::net, "curl: transfer finished " + url_.string());

	if (file_)
	{
		::FlushFileBuffers(file_.get());
		file_.reset();
	}

	if (interrupt_)
	{
		cx_.trace(context::net, "curl: " + url_.string() + " interrupted");
		return;
	}

	if (r == CURLE_OK)
	{
		long h = 0;
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &h);

		if (h == 200)
		{
			cx_.trace(context::net,
				"curl: http 200 " + url_.string() + ", "
				"transferred " + std::to_string(bytes_) + " bytes");

			ok_ = true;
			output_deleter.cancel();
		}
		else
		{
			cx_.error(context::net,
				"curl: http " + std::to_string(h) + " " + url_.string());
		}
	}
	else
	{
		cx_.error(context::net,
			std::string("curl: ") +
			curl_easy_strerror(r) + ", " + trim_copy(error_buffer) + " " +
			"(" + url_.string() + ")");
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

	if (self->interrupt_)
	{
		debug("downloader: interrupting");
		return (size * nmemb) + 1; // force failure
	}

	return size * nmemb;
}

void curl_downloader::on_write(char* ptr, std::size_t n) noexcept
{
	if (!file_)
	{
		op::create_directories(cx_, path_.parent_path());

		cx_.trace(context::net, "opening " + path_.string());

		HANDLE h = ::CreateFileA(
			path_.string().c_str(), GENERIC_WRITE, FILE_SHARE_READ,
			nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

		if (h == INVALID_HANDLE_VALUE)
		{
			const auto e = GetLastError();
			cx_.error(context::net, "failed to open " + path_.string(), e);
			interrupt_ = true;
			return;
		}

		file_.reset(h);
	}

	bytes_ += n;

	DWORD written = 0;
	if (!::WriteFile(file_.get(), ptr, static_cast<DWORD>(n), &written, nullptr))
	{
		const auto e = GetLastError();
		cx_.error(context::net, "failed to write to " + path_.string(), e);
		interrupt_ = true;
	}
}

int curl_downloader::on_progress_static(
	void* user, double, double, double, double) noexcept
{
	auto* self = static_cast<curl_downloader*>(user);

	if (self->interrupt_)
	{
		debug("downloader: interrupting");
		return 1;
	}

	return 0;
}

int curl_downloader::on_xfer_static(
	void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) noexcept
{
	auto* self = static_cast<curl_downloader*>(user);

	if (self->interrupt_)
	{
		debug("downloader: interrupting");
		return 1;
	}

	return 0;
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
		for_each_line(s, [&](auto&& line)
		{
			if (what.empty())
			{
				buffer = "curl: ";
				buffer.append(line);
			}
			else
			{
				buffer = "curl: ";
				buffer.append(what);
				buffer.append(": ");
				buffer.append(line);
			}

			cx_.dump(context::net, buffer);
		});
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

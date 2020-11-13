#include "pch.h"
#include "net.h"
#include "utility.h"
#include "core/conf.h"
#include "core/op.h"
#include "core/context.h"
#include "utility/threading.h"

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
			gcx().bail_out(context::net, "bad url '{}'", s_);

		char* buffer = nullptr;
		r = curl_url_get(h, CURLUPART_PATH, &buffer, 0);

		if (r != CURLUE_OK)
			gcx().bail_out(context::net, "bad url '{}'", s_);

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

void curl_downloader::start(const mob::url& u, const fs::path& path)
{
	url(u);
	file(path);
	start();
}

curl_downloader& curl_downloader::url(const mob::url& u)
{
	url_ = u;
	return *this;
}

curl_downloader& curl_downloader::file(const fs::path& file)
{
	path_ = file;
	return *this;
}

curl_downloader& curl_downloader::header(std::string name, std::string value)
{
	headers_.emplace_back(std::move(name), std::move(value));
	return *this;
}

curl_downloader& curl_downloader::start()
{
	ok_ = false;
	cx_.debug(context::net, "downloading {} to {}", url_, path_);

	if (conf::dry())
		return *this;

	thread_ = start_thread([&]{ run(); });
	return *this;
}

curl_downloader& curl_downloader::join()
{
	if (thread_.joinable())
		thread_.join();

	return *this;
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

const std::string& curl_downloader::output()
{
	return output_;
}

std::string curl_downloader::steal_output()
{
	std::string s = std::move(output_);
	output_.clear();
	return s;
}

void curl_downloader::run()
{
	cx_.trace(context::net, "curl: initializing {}", url_);

	auto* c = curl_easy_init();
	guard g([&]{ curl_easy_cleanup(c); });

	char error_buffer[CURL_ERROR_SIZE + 1] = {};
	const std::string ua =
		"ModOrganizer's " + mob_version() + " " + curl_version();

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
	curl_easy_setopt(c, CURLOPT_USERAGENT, ua.c_str());

	if (context::enabled(context::level::dump))
	{
		curl_easy_setopt(c, CURLOPT_DEBUGFUNCTION, on_debug_static);
		curl_easy_setopt(c, CURLOPT_DEBUGDATA, this);
		curl_easy_setopt(c, CURLOPT_VERBOSE, 1l);
	}


	// deletes the file in dtor unless cancel() is called
	std::unique_ptr<file_deleter> output_deleter;
	if (!path_.empty())
		output_deleter.reset(new file_deleter(cx_, path_));

	cx_.trace(context::net, "curl: performing {}", url_);
	const auto r = curl_easy_perform(c);
	cx_.trace(context::net, "curl: transfer finished {}", url_);

	if (file_)
	{
		::FlushFileBuffers(file_.get());
		file_.reset();
	}

	if (interrupt_)
	{
		cx_.trace(context::net, "curl: {} interrupted", url_);
		return;
	}

	if (r == CURLE_OK)
	{
		long h = 0;
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &h);

		if (h == 200)
		{
			// success

			cx_.trace(context::net,
				"curl: http 200 {}, transferred {} bytes",
				url_, bytes_);

			ok_ = true;

			if (output_deleter)
				output_deleter->cancel();
		}
		else
		{
			cx_.error(context::net, "curl: http {} {}", h, url_);
		}
	}
	else
	{
		cx_.error(context::net,
			"curl: {}, {} {}",
			curl_easy_strerror(r), trim_copy(error_buffer), url_);
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
	if (!create_file())
	{
		interrupt_ = true;
		return;
	}

	bool b = false;
	if (file_)
		b = write_file(ptr, n);
	else
		b = write_string(ptr, n);

	if (!b)
		interrupt_ = true;

	bytes_ += n;
}

bool curl_downloader::create_file()
{
	if (file_ || path_.empty())
		return true;

	// file is lazily created on first write

	op::create_directories(cx_, path_.parent_path());

	cx_.trace(context::net, "opening {}", path_);

	HANDLE h = ::CreateFileW(
		path_.native().c_str(), GENERIC_WRITE, FILE_SHARE_READ,
		nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

	if (h == INVALID_HANDLE_VALUE)
	{
		const auto e = GetLastError();

		cx_.error(context::net,
			"failed to open {}, {}", path_, error_message(e));

		return false;
	}

	file_.reset(h);
	return true;
}

bool curl_downloader::write_file(char* ptr, size_t n)
{
	DWORD written = 0;
	if (!::WriteFile(file_.get(), ptr, static_cast<DWORD>(n), &written, nullptr))
	{
		const auto e = GetLastError();

		cx_.error(context::net,
			"failed to write to {}, {}", path_, error_message(e));

		return false;
	}

	return true;
}

bool curl_downloader::write_string(char* ptr, size_t n)
{
	output_.append(ptr, n);
	return true;
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

// curl spams this stuff, make sure it's never logged
//
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

			cx_.dump(context::net, "{}", buffer);
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

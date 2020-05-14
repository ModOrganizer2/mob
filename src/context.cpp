#include "pch.h"
#include "context.h"
#include "utility.h"
#include "conf.h"
#include "tasks/task.h"
#include "tools/tools.h"

namespace mob::details
{

std::string converter<std::wstring>::convert(const std::wstring& s)
{
	return utf16_to_utf8(s);
}

std::string converter<fs::path>::convert(const fs::path& s)
{
	return utf16_to_utf8(s.native());
}

std::string converter<url>::convert(const url& u)
{
	return u.string();
}

}	// namespace


namespace mob
{

using hr_clock = std::chrono::high_resolution_clock;

static hr_clock::time_point g_start_time = hr_clock::now();
static std::vector<std::string> g_errors, g_warnings;
static handle_ptr g_log_file;
static std::mutex g_mutex;

console_color level_color(context::level lv)
{
	switch (lv)
	{
		case context::level::dump:
		case context::level::trace:
		case context::level::debug:
			return console_color::grey;

		case context::level::warning:
			return console_color::yellow;

		case context::level::error:
			return console_color::red;

		case context::level::info:
		default:
			return console_color::white;
	}
}

std::string reason_string(context::reason r)
{
	switch (r)
	{
		case context::bypass:        return "bypass";
		case context::redownload:    return "re-dl";
		case context::rebuild:       return "re-bd";
		case context::reextract:     return "re-ex";
		case context::interruption:  return "int";
		case context::cmd:			 return "cmd";
		case context::std_out:		 return "stdout";
		case context::std_err:		 return "stderr";
		case context::fs:            return (conf::dry() ? "fs-dry" : "fs");
		case context::net:           return "net";
		case context::generic:       return "";
		case context::conf:          return "conf";
		default:                     return "?";
	}
}

std::string task_name(const std::string& name)
{
	const std::size_t longest = 15;
	const std::size_t total = 1 + longest + 2; // '[x] '

	if (!name.empty())
		return pad_right("[" + name.substr(0, longest) + "]", total);
	else
		return std::string(total, ' ');
}

std::string tool_name(const tool* t)
{
	const std::size_t longest = 7;
	const std::size_t total = 1 + longest + 2; // '[x] '

	if (t && !t->name().empty())
		return pad_right("[" + t->name().substr(0, longest) + "]", total);
	else
		return std::string(total, ' ');
}

std::string prefix(context::reason r)
{
	const std::size_t longest = 7;
	const std::size_t total = 1 + longest + 2; // '[x] '

	const std::string rs = reason_string(r).substr(0, longest);

	if (!rs.empty())
		return pad_right("[" + rs + "] ", total);
	else
		return std::string(total, ' ');
}

std::string error_message(DWORD e)
{
	return std::error_code(
		static_cast<int>(e), std::system_category()).message();
}

std::string timestamp()
{
	const std::size_t max_length = 7; // 0000.00
	static thread_local char buffer[50];

	using namespace std::chrono;

	const auto d = hr_clock::now() - g_start_time;
	const auto ms = duration_cast<milliseconds>(d);
	const auto frac = ms.count() / 1000.0;

	const auto r = std::to_chars(
		std::begin(buffer), std::end(buffer), frac,
		std::chars_format::fixed, 2);

	const auto n = static_cast<std::size_t>(r.ptr - buffer);

	if (r.ec == std::errc())
		return pad_left(std::string(buffer, n), max_length, ' ');
	else
		return "?";
}


context::context(std::string task_name)
	: task_(std::move(task_name)), tool_(nullptr)
{
}

void context::set_tool(tool* t)
{
	tool_ = t;
}

const context* context::global()
{
	static thread_local context c("");
	return &c;
}

static bool log_enabled(context::level lv, int conf_lv)
{
	switch (lv)
	{
		case context::level::dump:
			return conf_lv > 5;

		case context::level::trace:
			return conf_lv > 4;

		case context::level::debug:
			return conf_lv > 3;

		case context::level::info:
			return conf_lv > 2;

		case context::level::warning:
			return conf_lv > 1;

		case context::level::error:
			return conf_lv > 0;

		default:
			return true;
	}
}

bool context::enabled(level lv)
{
	const int minimum_log_level =
		std::max(conf::output_log_level(), conf::file_log_level());

	return log_enabled(lv, minimum_log_level);
}

void context::set_log_file(const fs::path& p)
{
	if (!p.empty())
	{
		HANDLE h = CreateFileW(
			p.native().c_str(), GENERIC_WRITE, FILE_SHARE_READ,
			nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

		if (h == INVALID_HANDLE_VALUE)
		{
			const auto e = GetLastError();
			gcx().bail_out(context::generic,
				"failed to open log file {}, {}", p, error_message(e));
		}

		g_log_file.reset(h);
	}
}

void context::do_log_impl(
	bool bail, reason r, level lv, const std::string& utf8) const
{
	if (!bail && !enabled(lv))
		return;

	const auto ls = make_log_string(r, lv, utf8);

	if (bail)
	{
		emit_log(lv, ls + " (bailing out)");
		throw bailed(ls);
	}
	else
	{
		emit_log(lv, ls);
	}
}

void context::emit_log(level lv, const std::string& utf8) const
{
	std::scoped_lock lock(g_mutex);

	if (lv == level::error)
		g_errors.push_back(utf8);
	else if (lv == level::warning)
		g_warnings.push_back(utf8);

	if (log_enabled(lv, conf::output_log_level()))
	{
		auto c = level_color(lv);
		u8cout << utf8 << "\n";
	}

	if (g_log_file && log_enabled(lv, conf::file_log_level()))
	{
		DWORD written = 0;

		::WriteFile(
			g_log_file.get(), utf8.c_str(), static_cast<DWORD>(utf8.size()),
			&written, nullptr);

		::WriteFile(g_log_file.get(), "\r\n", 2, &written, nullptr);
	}
}

std::string context::make_log_string(reason r, level, std::string_view s) const
{
	std::ostringstream oss;

	oss
		<< task_name(task_)
		<< tool_name(tool_)
		<< prefix(r);

	switch (r)
	{
		case context::redownload:
			oss << s << " (happened because of --redownload)";
			break;

		case context::rebuild:
			oss << s << " (happened because of --rebuild)";
			break;

		case context::reextract:
			oss << s << " (happened because of --reextract)";
			break;

		case context::interruption:
			if (s.empty())
				oss << "interrupted";
			else
				oss << s;

			break;

		case context::cmd:
		case context::bypass:
		case context::std_out:
		case context::std_err:
		case context::fs:
		case context::net:
		case context::generic:
		case context::conf:
		default:
			oss << s;
			break;
	}

	return std::string(timestamp()) + " " + oss.str();
}

void dump_logs()
{
	if (!g_warnings.empty() || !g_errors.empty())
	{
		u8cout << "\n\nthere were problems:\n";

		{
			auto c = level_color(context::level::warning);
			for (auto&& s : g_warnings)
				u8cout << s << "\n";
		}

		{
			auto c = level_color(context::level::error);
			for (auto&& s : g_errors)
				u8cout << s << "\n";
		}
	}
}

}	// namespace

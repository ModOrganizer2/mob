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

const char* reason_string(context::reason r)
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

std::string error_message(DWORD id)
{
	wchar_t* message = nullptr;

	const auto ret = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		id,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPWSTR>(&message),
		0, NULL);

	std::wstring s;

	std::wostringstream oss;
	oss << L"0x" << std::hex << id;

	if (ret == 0 || !message) {
		s = oss.str();
	} else {
		s = trim_copy(message) + L" (" + oss.str() + L")";
	}

	LocalFree(message);

	return utf16_to_utf8(s);
}

std::chrono::nanoseconds timestamp()
{
	return (hr_clock::now() - g_start_time);
}

std::string_view timestamp_string()
{
	static thread_local char buffer[50];

	using namespace std::chrono;

	const auto ms = duration_cast<milliseconds>(timestamp());
	const auto frac = static_cast<float>(ms.count()) / 1000.0;

	const auto r = std::to_chars(
		std::begin(buffer), std::end(buffer), frac,
		std::chars_format::fixed, 2);

	const auto n = static_cast<std::size_t>(r.ptr - buffer);

	if (r.ec == std::errc())
		return {buffer, n};
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
	bool bail, reason r, level lv, std::string_view utf8) const
{
	std::string_view ls = make_log_string(r, lv, utf8);

	if (bail)
	{
		emit_log(lv, std::string(ls) + " (bailing out)");
		throw bailed(std::string(ls));
	}
	else
	{
		emit_log(lv, ls);
	}
}

void context::emit_log(level lv, std::string_view utf8) const
{
	std::scoped_lock lock(g_mutex);

	if (log_enabled(lv, conf::output_log_level()))
	{
		auto c = level_color(lv);
		u8cout.write_ln(utf8);
	}

	if (g_log_file && log_enabled(lv, conf::file_log_level()))
	{
		DWORD written = 0;

		::WriteFile(
			g_log_file.get(), utf8.data(), static_cast<DWORD>(utf8.size()),
			&written, nullptr);

		::WriteFile(g_log_file.get(), "\r\n", 2, &written, nullptr);
	}

	if (lv == level::error)
		g_errors.emplace_back(utf8);
	else if (lv == level::warning)
		g_warnings.emplace_back(utf8);
}

void append_brackets(std::string& s, std::string_view what, std::size_t total)
{
	if (what.empty())
	{
		s.append(total, ' ');
	}
	else
	{
		s.append(1, '[');
		s.append(what);
		s.append(1, ']');

		const std::size_t written = 1 + what.size() + 1;  // "[x]"

		if (written < total)
			s.append(total - written, ' ');
	}
}

void append(std::string& s, std::string_view what, std::size_t total)
{
	if (what.empty())
	{
		s.append(total, ' ');
	}
	else
	{
		s.append(what);

		const std::size_t written = what.size();  // "x"

		if (written < total)
			s.append(total - written, ' ');
	}
}

std::string_view context::make_log_string(reason r, level, std::string_view s) const
{
	const std::size_t total_timestamp = 8; // '0000.00 '

	const std::size_t longest_task_name = 15;
	const std::size_t total_task_name = 1 + longest_task_name + 2; // '[x] '

	const std::size_t longest_tool_name = 7;
	const std::size_t total_tool_name = 1 + longest_tool_name + 2; // '[x] '

	const std::size_t longest_prefix = 7;
	const std::size_t total_prefix = 1 + longest_prefix + 2; // '[x] '

	static thread_local std::string ls;

	ls.reserve(
		total_timestamp +                // timestamp
		total_task_name +
		total_tool_name +
		total_prefix +
		s.size() +
		50);               // possible additional stuff

	ls.clear();

	append(ls, timestamp_string(), total_timestamp);
	ls.append(1, ' ');
	append_brackets(ls, task_.substr(0, longest_task_name), total_task_name);

	if (tool_)
		append_brackets(ls, tool_->name().substr(0, longest_tool_name), total_tool_name);
	else
		ls.append(total_tool_name, ' ');

	append_brackets(ls, reason_string(r), total_prefix);

	ls.append(s);

	switch (r)
	{
		case context::redownload:
			ls.append(" (happened because of --redownload)");
			break;

		case context::rebuild:
			ls.append(" (happened because of --rebuild)");
			break;

		case context::reextract:
			ls.append(" (happened because of --reextract)");
			break;

		case context::interruption:
			if (s.empty())
				ls.append("interrupted");

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
			break;
	}

	return ls;
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

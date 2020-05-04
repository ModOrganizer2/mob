#include "pch.h"
#include "context.h"
#include "utility.h"
#include "conf.h"
#include "tasks/task.h"
#include "tools/tools.h"

namespace mob
{

using hr_clock = std::chrono::high_resolution_clock;

static hr_clock::time_point g_start_time = hr_clock::now();
static std::vector<std::string> g_errors, g_warnings;
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
		case context::fs:            return "fs";
		case context::net:           return "net";
		case context::generic:       return "";
		default:                     return "?";
	}
}

std::string task_name(const task* t)
{
	const std::size_t longest = 7;
	const std::size_t total = 1 + longest + 2; // '[x] '

	if (t && !t->name().empty())
		return pad_right("[" + t->name().substr(0, longest) + "]", total);
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

const context* context::global()
{
	static thread_local context c;
	return &c;
}

bool context::enabled(reason, level lv)
{
	switch (lv)
	{
		case level::dump:
			return conf::log_dump();

		case level::trace:
			return conf::log_trace();

		case level::debug:
			return conf::log_debug();

		case level::info:
			return conf::log_info();

		case level::warning:
			return conf::log_warning();

		case level::error:
			return conf::log_error();

		default:
			return true;
	}
}

void context::log(reason r, level lv, std::string_view s) const
{
	if (!enabled(r, lv))
		return;

	const auto ls = make_log_string(r, lv, s);
	do_log(lv, ls);
}

void context::log(reason r, level lv, std::string_view s, DWORD e) const
{
	log(r, lv, std::string(s) + ", " + error_message(e));
}

void context::log(reason r, level lv, std::string_view s, const std::error_code& ec) const
{
	log(r, lv, std::string(s) + ", " + ec.message());
}

void context::bail_out(reason r, std::string_view s) const
{
	const auto ls = make_log_string(r, level::error, s);
	do_log(level::error, ls + " (bailing out)");

	throw bailed(ls);
}

void context::bail_out(reason r, std::string_view s, DWORD e) const
{
	bail_out(r, std::string(s) + ", " + error_message(e));
}

void context::bail_out(reason r, std::string_view s, const std::error_code& ec) const
{
	bail_out(r, std::string(s) + ", " + ec.message());
}

void context::do_log(level lv, const std::string& s) const
{
	{
		std::scoped_lock lock(g_mutex);
		auto c = level_color(lv);

		std::cout << s << "\n";

		if (lv == level::error)
			g_errors.push_back(s);
		else if (lv == level::warning)
			g_warnings.push_back(s);
	}
}

std::string context::make_log_string(reason r, level, std::string_view s) const
{
	std::ostringstream oss;

	oss
		<< task_name(task)
		<< tool_name(tool)
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
		default:
			oss << s;
			break;
	}

	return std::string(timestamp()) + " " + oss.str();
}

void dump_logs()
{
	if (!g_warnings.empty())
	{
		auto c = level_color(context::level::warning);
		std::cout << "\n\nthere were warnings:\n";

		for (auto&& s : g_warnings)
			std::cout << s << "\n";
	}

	if (!g_errors.empty())
	{
		auto c = level_color(context::level::error);
		std::cout << "\n\nthere were errors:\n";

		for (auto&& s : g_errors)
			std::cout << s << "\n";
	}
}

}	// namespace

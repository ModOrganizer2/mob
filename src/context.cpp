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

enum class color_methods
{
	none = 0,
	ansi,
	console
};

enum class colors
{
	white,
	grey,
	yellow,
	red
};


static color_methods g_color_method = []
{
	DWORD d = 0;
	if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &d))
	{
		if ((d & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0)
			return color_methods::console;
		else
			return color_methods::ansi;
	}

	return color_methods::none;
}();


class color
{
public:
	color()
		: reset_(false), old_atts_(0)
	{
	}

	color(colors c)
		: reset_(false), old_atts_(0)
	{
		if (g_color_method == color_methods::ansi)
		{
			switch (c)
			{
				case colors::white:
					break;

				case colors::grey:
					reset_ = true;
					std::cout << "\033[38;2;150;150;150m";
					break;

				case colors::yellow:
					reset_ = true;
					std::cout << "\033[38;2;240;240;50m";
					break;

				case colors::red:
					reset_ = true;
					std::cout << "\033[38;2;240;50;50m";
					break;
			}
		}
		else if (g_color_method == color_methods::console)
		{
			CONSOLE_SCREEN_BUFFER_INFO bi = {};
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bi);
			old_atts_ = bi.wAttributes;

			WORD atts = 0;

			switch (c)
			{
				case colors::white:
					break;

				case colors::grey:
					reset_ = true;
					atts = FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED;
					break;

				case colors::yellow:
					reset_ = true;
					atts = FOREGROUND_GREEN|FOREGROUND_RED;
					break;

				case colors::red:
					reset_ = true;
					atts = FOREGROUND_RED;
					break;
			}

			if (atts != 0)
				SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), atts);
		}
	}

	~color()
	{
		if (!reset_)
			return;

		if (g_color_method == color_methods::ansi)
		{
			std::cout << "\033[39m\033[49m";
		}
		else if (g_color_method == color_methods::console)
		{
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), old_atts_);
		}
	}

	color(const color&) = delete;
	color& operator=(const color&) = delete;

private:
	bool reset_;
	WORD old_atts_;
};


color color_for_level(level lv)
{
	if (g_color_method == color_methods::none)
		return {};

	switch (lv)
	{
		case level::error:
		case level::bail:
			return colors::red;

		case level::warning:
			return colors::yellow;

		case level::debug:
		case level::trace:
		case level::dump:
			return colors::grey;

		case level::info:
		default:
			return colors::white;
	}
}


std::string error_message(DWORD e)
{
	return std::error_code(
		static_cast<int>(e), std::system_category()).message();
}


std::vector<std::string> g_errors, g_warnings;
std::mutex g_log_mutex;

std::string_view timestamp()
{
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
		return {buffer, n};
	else
		return "?";
}

void out(level lv, const std::string& s)
{

	{
		std::scoped_lock lock(g_log_mutex);
		auto c = color_for_level(lv);

		std::cout << timestamp() << " " << s << "\n";

		if (lv == level::error || lv == level::bail)
			g_errors.push_back(s);
		else if (lv == level::warning)
			g_warnings.push_back(s);
	}

	if (lv == level::bail)
		throw bailed(s);
}

void out(level lv, const std::string& s, DWORD e)
{
	out(lv, s + ", " + error_message(e));
}

void out(level lv, const std::string& s, const std::error_code& e)
{
	out(lv, s + ", " + e.message());
}

const context* context::global()
{
	static thread_local context c;
	return &c;
}

bool context::enabled(reasons r)
{
	switch (r)
	{
		case bypass:
		case redownload:
		case rebuild:
		case reextract:
		case interrupted:
		case op:
		case net:
		case cmd:
		case std_out:
		case std_err:
			return conf::verbose();

		case trace:
		case op_trace:
		case net_trace:
		case cmd_trace:
		case std_out_trace:
		case std_err_trace:
			return conf::trace();

		case net_dump:
			return conf::more_trace();

		case info:
		case warning:
		case error:
		case bailing:
		default:
			return true;
	}
}

std::string reason_string(context::reasons r)
{
	switch (r)
	{
		case context::bypass:        return "bypass";
		case context::redownload:    return "re-dl";
		case context::rebuild:       return "re-bd";
		case context::reextract:     return "re-ex";
		case context::interrupted:   return "int";
		case context::cmd:			 return "cmd";
		case context::cmd_trace:	 return "cmd+";
		case context::std_out:		 return "sout";
		case context::std_out_trace: return "sout+";
		case context::std_err:		 return "serr";
		case context::std_err_trace: return "serr+";
		case context::op:            return "fs";
		case context::net:           return "net";
		case context::trace:         return "trace";
		case context::op_trace:      return "op+";
		case context::net_trace:     return "net+";
		case context::net_dump:      return "net++";
		case context::info:          return "info";
		case context::warning:       return "warn";
		case context::error:         return "err";
		case context::bailing:       return "bail";
		default:                     return "?";
	}
}

std::string pad(std::string s, std::size_t n)
{
	if (s.size() < n)
		s.append(n - s.size() , ' ');

	return s;
}

std::string task_name(const task* t)
{
	const std::size_t longest = 7;
	const std::size_t total = 1 + longest + 2; // '[x] '

	if (t)
		return pad("[" + t->name() + "]", total);
	else
		return std::string(total, ' ');
}

std::string tool_name(const tool* t)
{
	const std::size_t longest = 7;
	const std::size_t total = 1 + longest + 2; // '[x] '

	if (t)
		return pad("[" + t->name() + "]", total);
	else
		return std::string(total, ' ');
}

std::string prefix(context::reasons r)
{
	const std::size_t longest = 7;
	const std::size_t total = 1 + longest + 2; // '[x] '

	return pad("[" + reason_string(r) + "] ", total);
}

context::cx_log context::fix_log(reasons r, std::string_view s) const
{
	auto lv = level::info;
	std::ostringstream oss;

	oss << task_name(task);
	oss << tool_name(tool);

	oss << prefix(r);

	switch (r)
	{
		case context::trace:
		case context::op_trace:
		case context::net_trace:
		case context::bypass:
		case context::std_out:
		case context::cmd_trace:
		case context::std_out_trace:
		case context::std_err:
		case context::std_err_trace:
			oss << s;
			lv = level::trace;
			break;

		case context::cmd:
			oss << "> " << s;
			lv = level::trace;
			break;

		case context::op:
		case context::net:
		case context::info:
			oss << s;
			lv = level::info;
			break;

		case context::warning:
			oss << s;
			lv = level::warning;
			break;

		case context::error:
			oss << s;
			lv = level::error;
			break;

		case context::net_dump:
			oss << s;
			lv = level::dump;
			break;

		case context::bailing:
			oss << s;
			lv = level::bail;
			break;

		case context::redownload:
			oss << s << " (happened because of --redownload)";
			lv = level::trace;
			break;

		case context::rebuild:
			oss << s << " (happened because of --rebuild)";
			lv = level::trace;
			break;

		case context::reextract:
			oss << s << " (happened because of --reextract)";
			lv = level::trace;
			break;

		case context::interrupted:
			if (s.empty())
				oss << "interrupted";
			else
				oss << s << " (interrupted)";

			lv = level::trace;
			break;

		default:
			oss << s;
			break;
	}

	return {lv, oss.str()};
}


void dump_logs()
{
	if (!g_warnings.empty())
	{
		auto c = color_for_level(level::warning);
		std::cout << "\n\nthere were warnings:\n";

		for (auto&& s : g_warnings)
			std::cout << " - " << s << "\n";
	}

	if (!g_errors.empty())
	{
		auto c = color_for_level(level::error);
		std::cout << "\n\nthere were errors:\n";

		for (auto&& s : g_errors)
			std::cout << " - " << s << "\n";
	}
}

}	// namespace

#include "pch.h"
#include "context.h"
#include "utility.h"
#include "conf.h"
#include "tasks/task.h"
#include "tools/tools.h"

namespace mob
{

struct color
{
	color(int r, int g, int b)
	{
		std::cout << "\033[38;2;" << r << ";" << g << ";" << b << "m";
	}

	~color()
	{
		std::cout << "\033[39m\033[49m";
	}

	color(const color&) = delete;
	color& operator=(const color&) = delete;
};

std::unique_ptr<color> color_for_level(level lv)
{
	switch (lv)
	{
		case level::error:
		case level::bail:
			return std::make_unique<color>(240, 50, 50);

		case level::warning:
			return std::make_unique<color>(240, 240, 50);

		case level::debug:
		case level::trace:
		case level::dump:
			return std::make_unique<color>(150, 150, 150);

		case level::info:
		default:
			return {};
	}
}


std::string error_message(DWORD e)
{
	return std::error_code(
		static_cast<int>(e), std::system_category()).message();
}


std::vector<std::string> g_errors, g_warnings;
std::mutex g_log_mutex;

void out(level lv, const std::string& s)
{
	//if (lv == level::debug && !conf::verbose())
	//	return;

	{
		std::scoped_lock lock(g_log_mutex);
		auto c = color_for_level(lv);

		std::cout << s << "\n";

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

const context* context::dummy()
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
		case interrupted:
		case op:
		case net:
			return conf::verbose();

		case trace:
		case op_trace:
		case net_trace:
			return conf::trace();

		case net_dump:
			return conf::more_trace();

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
		case context::bypass:       return "bypass";
		case context::redownload:   return "re-dl";
		case context::rebuild:      return "re-bd";
		case context::interrupted:  return "int";
		case context::op:           return "fs";
		case context::net:          return "net";
		case context::trace:        return "trace";
		case context::op_trace:     return "op+";
		case context::net_trace:    return "net+";
		case context::net_dump:     return "net++";
		case context::error:        return "err";
		case context::bailing:      return "bail";
		default:                    return "?";
	}
}

std::string prefix(context::reasons r)
{
	const std::size_t longest = 6;  // bypass

									// '[x] '
	const std::size_t total = 1 + longest + 2;

	std::string ss = "[" + reason_string(r) + "] ";
	if (ss.size() < total)
		ss += std::string(total - ss.size() , ' ');

	return ss;
}

context::cx_log context::fix_log(reasons r, const std::string& s) const
{
	auto lv = level::info;
	std::ostringstream oss;

	if (task)
		oss << "[" << task->name() << "] ";
	else
		oss << "     ";

	if (tool)
		oss << "[" << tool->name() << "] ";
	else
		oss << "     ";

	oss << prefix(r);

	switch (r)
	{
		case context::trace:
			oss << s;
			lv = level::trace;
			break;

		case context::bypass:
			oss << s;
			lv = level::trace;
			break;

		case context::redownload:
			oss << s << " (happened because of --redownload)";
			lv = level::trace;
			break;

		case context::rebuild:
			oss << s << " (happened because of --rebuild)";
			lv = level::trace;
			break;

		case context::interrupted:
			if (s.empty())
				oss << "interrupted";
			else
				oss << s << " (interrupted)";

			lv = level::trace;
			break;

		case context::op:
			oss << s;
			break;

		case context::op_trace:
			oss << s;
			lv = level::trace;
			break;

		case context::net:
			oss << s;
			break;

		case context::net_trace:
			oss << s;
			lv = level::trace;
			break;

		case context::net_dump:
			oss << s;
			lv = level::dump;
			break;

		case context::error:
			oss << s;
			lv = level::error;
			break;

		case context::bailing:
			oss << s;
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

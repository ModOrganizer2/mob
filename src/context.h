#pragma once

namespace mob
{

class task;
class tool;

enum class level
{
	dump, trace, debug, info, warning, error, bail
};


std::string error_message(DWORD e);



void out(level lv, const std::string& s);
void out(level lv, const std::string& s, DWORD e);
void out(level lv, const std::string& s, const std::error_code& e);

void dump_logs();

template <class... Args>
[[noreturn]] void bail_out(Args&&... args)
{
	out(level::bail, std::forward<Args>(args)...);
}

template <class... Args>
void error(Args&&... args)
{
	out(level::error, std::forward<Args>(args)...);
}

template <class... Args>
void warn(Args&&... args)
{
	out(level::warning, std::forward<Args>(args)...);
}

template <class... Args>
void info(Args&&... args)
{
	out(level::info, std::forward<Args>(args)...);
}

template <class... Args>
void debug(Args&&... args)
{
	out(level::debug, std::forward<Args>(args)...);
}


class context
{
public:
	enum reasons
	{
		// generic trace log
		trace = 1,

		// an action was bypassed because it was already done
		bypass,

		// an action was done because the --redownload option was set
		redownload,

		// an action was done because the --rebuild option was set
		rebuild,

		// an action was done because the --reextract option was set
		reextract,

		// something returned early because it was interrupted
		interrupted,

		// command line of a process
		cmd,
		cmd_trace,

		// output of a process
		std_out,
		std_out_trace,
		std_err,
		std_err_trace,

		// a filesystem action
		op,
		op_trace,

		// a network action
		net,
		net_trace,
		net_dump,

		// generic
		info,
		warning,
		error,

		// unrecoverable error, used by bail_out
		bailing
	};

	static const context* global();
	static bool enabled(reasons r);

	context() = default;
	context(const context&) = delete;
	context& operator=(const context&) = delete;

	task* task = nullptr;
	tool* tool = nullptr;

	template <class... Args>
	void log(reasons r, std::string_view s, Args&&... args) const
	{
		if (!enabled(r))
			return;

		auto cxl = fix_log(r, s);
		mob::out(cxl.lv, cxl.s, std::forward<Args>(args)...);
	}

	template <class... Args>
	[[noreturn]] void bail_out(std::string_view s, Args&&... args) const
	{
		auto cxl = fix_log(bailing, s);
		mob::bail_out(cxl.s, std::forward<Args>(args)...);
	}

private:
	struct cx_log
	{
		level lv;
		std::string s;
	};

	cx_log fix_log(reasons r, std::string_view s) const;
};

}	// namespace

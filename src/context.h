#pragma once

namespace mob
{

class task;
class tool;

class context
{
public:
	enum reason
	{
		// generic
		generic,

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

		// output of a process
		std_out,
		std_err,

		// a filesystem action
		fs,

		// a network action
		net,

		// unrecoverable error, used by bail_out
		bailing
	};

	enum class level
	{
		dump = 1,
		trace,
		debug,
		info,
		warning,
		error,
	};

	static const context* global();
	static bool enabled(reason r, level lv);

	context() = default;
	context(const context&) = delete;
	context& operator=(const context&) = delete;

	task* task = nullptr;
	tool* tool = nullptr;

	void log(reason r, level lv, std::string_view s) const;
	void log(reason r, level lv, std::string_view s, DWORD e) const;
	void log(reason r, level lv, std::string_view s, const std::error_code& ec) const;

	template <class... Args>
	void dump(reason r, std::string_view s, Args&&... args) const
	{
		log(r, level::dump, s, std::forward<Args>(args)...);
	}

	template <class... Args>
	void trace(reason r, std::string_view s, Args&&... args) const
	{
		log(r, level::trace, s, std::forward<Args>(args)...);
	}

	template <class... Args>
	void debug(reason r, std::string_view s, Args&&... args) const
	{
		log(r, level::debug, s, std::forward<Args>(args)...);
	}

	template <class... Args>
	void info(reason r, std::string_view s, Args&&... args) const
	{
		log(r, level::info, s, std::forward<Args>(args)...);
	}

	template <class... Args>
	void warning(reason r, std::string_view s, Args&&... args) const
	{
		log(r, level::warning, s, std::forward<Args>(args)...);
	}

	template <class... Args>
	void error(reason r, std::string_view s, Args&&... args) const
	{
		log(r, level::error, s, std::forward<Args>(args)...);
	}

	[[noreturn]] void bail_out(reason r, std::string_view s) const;
	[[noreturn]] void bail_out(reason r, std::string_view s, DWORD e) const;
	[[noreturn]] void bail_out(reason r, std::string_view s, const std::error_code& ec) const;

private:
	std::string make_log_string(reason r, level lv, std::string_view s) const;
	void do_log(level lv, const std::string& s) const;
};


inline const context& gcx()
{
	return *context::global();
}

void dump_logs();


// temp

inline void out(context::level lv, const std::string& s)
{
	gcx().log(context::generic, lv, s);
}

inline void out(context::level lv, const std::string& s, DWORD e)
{
	gcx().log(context::generic, lv, s, e);
}

inline void out(context::level lv, const std::string& s, const std::error_code& ec)
{
	gcx().log(context::generic, lv, s, ec);
}

template <class... Args>
[[noreturn]] void bail_out(Args&&... args)
{
	gcx().bail_out(context::generic, std::forward<Args>(args)...);
}


template <class... Args>
void error(Args&&... args)
{
	gcx().error(context::generic, std::forward<Args>(args)...);
}

template <class... Args>
void warn(Args&&... args)
{
	gcx().warn(context::generic, std::forward<Args>(args)...);
}

template <class... Args>
void info(Args&&... args)
{
	gcx().info(context::generic, std::forward<Args>(args)...);
}

template <class... Args>
void debug(Args&&... args)
{
	gcx().debug(context::generic, std::forward<Args>(args)...);
}

}	// namespace

#pragma once

#include "utility.h"

namespace mob
{
	class url;
}


namespace mob::details
{

// T to std::string converters
//
// those are kept in this namespace so they don't leak all over the place;
// they're used directly by doLog() below

template <class T, class=void>
struct converter
{
	static const T& convert(const T& t)
	{
		return t;
	}
};

template <>
struct converter<std::wstring>
{
	static std::string convert(const std::wstring& s);
};

template <>
struct converter<fs::path>
{
	static std::string convert(const fs::path& s);
};

template <>
struct converter<url>
{
	static std::string convert(const url& u);
};

template <class T>
struct converter<T, std::enable_if_t<std::is_enum_v<T>>>
{
	static std::string convert(T e)
	{
		return std::to_string(static_cast<std::underlying_type_t<T>>(e));
	}
};

}	// namespace


namespace mob
{

class task;
class tool;

// time since mob started
//
std::chrono::nanoseconds timestamp();

std::string error_message(DWORD e);

class context
{
public:
	enum reason
	{
		// generic
		generic,

		// a configuration action
		conf,

		// something was bypassed because it was already done
		bypass,

		// something was done because the --redownload option was set
		redownload,

		// something was done because the --rebuild option was set
		rebuild,

		// something was done because the --reextract option was set
		reextract,

		// something was done in case of interruption or because something
		// was interrupted
		interruption,

		// command line of a process
		cmd,

		// output of a process
		std_out,
		std_err,

		// a filesystem action
		fs,

		// a network action
		net,
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
	static bool enabled(level lv);
	static void set_log_file(const fs::path& p);

	context(std::string task_name);

	void set_tool(tool* t);

	template <class... Args>
	void log(reason r, level lv, const char* f, Args&&... args) const
	{
		do_log(false, r, lv, f, std::forward<Args>(args)...);
	}

	void log_string(reason r, level lv, std::string_view s) const
	{
		do_log_string(false, r, lv, s);
	}

	template <class... Args>
	void dump(reason r, const char* f, Args&&... args) const
	{
		do_log(false, r, level::dump, f, std::forward<Args>(args)...);
	}

	template <class... Args>
	void trace(reason r, const char* f, Args&&... args) const
	{
		do_log(false, r, level::trace, f, std::forward<Args>(args)...);
	}

	template <class... Args>
	void debug(reason r, const char* f, Args&&... args) const
	{
		do_log(false, r, level::debug, f, std::forward<Args>(args)...);
	}

	template <class... Args>
	void info(reason r, const char* f, Args&&... args) const
	{
		do_log(false, r, level::info, f, std::forward<Args>(args)...);
	}

	template <class... Args>
	void warning(reason r, const char* f, Args&&... args) const
	{
		do_log(false, r, level::warning, f, std::forward<Args>(args)...);
	}

	template <class... Args>
	void error(reason r, const char* f, Args&&... args) const
	{
		do_log(false, r, level::error, f, std::forward<Args>(args)...);
	}

	template <class... Args>
	[[noreturn]] void bail_out(reason r, const char* f, Args&&... args) const
	{
		do_log(true, r, level::error, f, std::forward<Args>(args)...);
	}

private:
	std::string task_;
	const tool* tool_;

	void do_log_string(bool bail, reason r, level lv, std::string_view s) const
	{
		if (!bail && !enabled(lv))
			return;

		do_log_impl(bail, r, lv, s);
	}

	template <class... Args>
	void do_log(bool bail, reason r, level lv, const char* f, Args&&... args) const
	{
		if (!bail && !enabled(lv))
			return;

		try
		{
			const std::string utf8 = ::fmt::format(
				f,
				details::converter<std::decay_t<Args>>::convert(
					std::forward<Args>(args))...);

			do_log_impl(bail, r, lv, utf8);
		}
		catch(std::exception&)
		{
			std::wstring s;

			const char* p = f;
			while (*p)
			{
				s += (wchar_t)*p;
				++p;
			}

			std::wcerr << "bad format string '" << s << "'\n";
			MOB_ASSERT(false, "bad format string");
		}
	}

	std::string_view make_log_string(reason r, level lv, std::string_view s) const;
	void do_log_impl(bool bail, reason r, level lv, std::string_view utf8) const;
	void emit_log(level lv, std::string_view utf8) const;
};


inline const context& gcx()
{
	return *context::global();
}

void dump_logs();


// temp

inline void out(context::level lv, const std::string& s)
{
	gcx().log(context::generic, lv, "{}", s);
}

inline void out(context::level lv, const std::string& s, DWORD e)
{
	gcx().log(context::generic, lv, "{}, {}", s, error_message(e));
}

inline void out(context::level lv, const std::string& s, const std::error_code& ec)
{
	gcx().log(context::generic, lv, "{}, {}", s, ec.message());
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

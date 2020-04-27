#pragma once

namespace builder
{

class bailed
{
public:
	bailed(std::string s)
		: s_(std::move(s))
	{
	}

	const char* what() const
	{
		return s_.c_str();
	}

private:
	std::string s_;
};


struct handle_deleter
{
	using pointer = HANDLE;

	void operator()(HANDLE h)
	{
		if (h != INVALID_HANDLE_VALUE) {
			::CloseHandle(h);
		}
	}
};

using handle_ptr = std::unique_ptr<HANDLE, handle_deleter>;



enum class level
{
	debug, info, warning, error, bail
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


std::string env(const std::string& name);

std::string redir_nul();

std::string read_text_file(const fs::path& p);

std::string replace_all(
	std::string s, const std::string& from, const std::string& to);

}	// namespace

#pragma once

namespace builder
{

class bailed {};

enum class level
{
	debug, info, warning, error
};


std::string error_message(DWORD e);

void out(level lv, const std::string& s);
void out(level lv, const std::string& s, DWORD e);
void out(level lv, const std::string& s, const std::error_code& e);

template <class... Args>
[[noreturn]] void bail_out(Args&&... args)
{
	out(level::error, std::forward<Args>(args)...);
	throw bailed();
}

template <class... Args>
void error(Args&&... args)
{
	out(level::error, std::forward<Args>(args)...);
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


std::string redir_nul();

std::string read_text_file(const fs::path& p);

std::string replace_all(
	std::string s, const std::string& from, const std::string& to);

}	// namespace

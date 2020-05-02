#include "pch.h"
#include "utility.h"
#include "conf.h"
#include "op.h"
#include "net.h"
#include "process.h"

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
	if (lv == level::debug && !conf::verbose())
		return;

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


std::string read_text_file(const fs::path& p)
{
	debug("reading " + p.string());

	std::string s;

	std::ifstream in(p);
	if (!in)
		bail_out("can't read from " + p.string() + "'");

	in.seekg(0, std::ios::end);
	s.resize(static_cast<std::size_t>(in.tellg()));
	in.seekg(0, std::ios::beg);
	in.read(&s[0], static_cast<std::streamsize>(s.size()));

	return s;
}

std::string replace_all(
	std::string s, const std::string& from, const std::string& to)
{
	for (;;)
	{
		const auto pos = s.find(from);
		if (pos == std::string::npos)
			break;

		s.replace(pos, from.size(), to);
	}

	return s;
}

std::string join(const std::vector<std::string>& v, const std::string& sep)
{
	std::string s;

	for (auto&& e : v)
	{
		if (!s.empty())
			s += sep;

		s += e;
	}

	return s;
}


file_deleter::file_deleter(fs::path p)
	: p_(std::move(p)), delete_(true)
{
}

file_deleter::~file_deleter()
{
	if (delete_)
		delete_now();
}

void file_deleter::delete_now()
{
	if (fs::exists(p_))
		op::delete_file(p_);
}

void file_deleter::cancel()
{
	delete_ = false;
}


directory_deleter::directory_deleter(fs::path p)
	: p_(std::move(p)), delete_(true)
{
}

directory_deleter::~directory_deleter()
{
	if (delete_)
		delete_now();
}

void directory_deleter::delete_now()
{
	if (fs::exists(p_))
		op::delete_directory(p_);
}

void directory_deleter::cancel()
{
	delete_ = false;
}

}	// namespace

#include "pch.h"
#include "utility.h"
#include "conf.h"
#include "op.h"

namespace builder
{

std::string error_message(DWORD e)
{
	return std::error_code(
		static_cast<int>(e), std::system_category()).message();
}

void out(level lv, const std::string& s)
{
	static std::mutex m;

	if (lv == level::debug && !conf::verbose())
		return;

	std::scoped_lock lock(m);
	std::cout << s << "\n";
}

void out(level lv, const std::string& s, DWORD e)
{
	out(lv, s + ", " + error_message(e));
}

void out(level lv, const std::string& s, const std::error_code& e)
{
	out(lv, s + ", " + e.message());
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

}	// namespace

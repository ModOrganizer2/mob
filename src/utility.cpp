#include "pch.h"
#include "utility.h"
#include "conf.h"
#include "op.h"
#include "net.h"
#include "process.h"
#include "context.h"

namespace mob
{

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
	op::delete_file(p_, op::optional);
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
	op::delete_directory(p_, op::optional);
}

void directory_deleter::cancel()
{
	delete_ = false;
}

}	// namespace

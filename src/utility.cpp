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


file_deleter::file_deleter(fs::path p, const context* cx)
	: cx_(cx), p_(std::move(p)), delete_(true)
{
	cx_->log(
		context::op_trace,
		"will delete " + p_.string() + " if things go wrong");
}

file_deleter::~file_deleter()
{
	if (delete_)
		delete_now();
}

void file_deleter::delete_now()
{
	cx_->log(
		context::op,
		"something went wrong, deleting " + p_.string());

	op::delete_file(p_, op::optional);
}

void file_deleter::cancel()
{
	cx_->log(
		context::op,
		"everything okay, keeping " + p_.string());

	delete_ = false;
}


directory_deleter::directory_deleter(fs::path p, const context* cx)
	: cx_(cx), p_(std::move(p)), delete_(true)
{
	cx_->log(
		context::op_trace,
		"will delete " + p_.string() + " if things go wrong");
}

directory_deleter::~directory_deleter()
{
	if (delete_)
		delete_now();
}

void directory_deleter::delete_now()
{
	cx_->log(
		context::op,
		"something went wrong, deleting " + p_.string());

	op::delete_directory(p_, op::optional);
}

void directory_deleter::cancel()
{
	cx_->log(
		context::op,
		"everything okay, keeping " + p_.string());

	delete_ = false;
}


interruption_file::interruption_file(
	fs::path dir, std::string name, const context* cx)
		: cx_(cx), dir_(std::move(dir)), name_(std::move(name))
{
	if (fs::exists(file()))
	{
		cx_->log(
			context::trace,
			"found interrupt file " + file().string());
	}
}

bool interruption_file::exists() const
{
	return fs::exists(file());
}

fs::path interruption_file::file() const
{
	return dir_ / ("_mo_interrupted_" + name_);
}

void interruption_file::create()
{
	cx_->log(context::trace, "creating interrupt file " + file().string());
	op::touch(file(), cx_);
}

void interruption_file::remove()
{
	cx_->log(context::trace, "removing interrupt file " + file().string());
	op::delete_file(file(), op::noflags, cx_);
}

}	// namespace

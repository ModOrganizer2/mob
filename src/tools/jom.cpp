#include "pch.h"
#include "tools.h"
#include "../core/conf.h"
#include "../core/process.h"

namespace mob
{

jom::jom()
	: basic_process_runner("jom"), arch_(arch::def)
{
}

fs::path jom::binary()
{
	return conf().tool().get("jom");
}

jom& jom::path(const fs::path& p)
{
	cwd_ = p;
	return *this;
}

jom& jom::target(const std::string& s)
{
	target_ = s;
	return *this;
}

jom& jom::def(const std::string& s)
{
	def_.push_back(s);
	return *this;
}

jom& jom::flag(flags_t f)
{
	flags_ = f;
	return *this;
}

jom& jom::architecture(arch a)
{
	arch_ = a;
	return *this;
}

int jom::result() const
{
	return exit_code();
}

void jom::do_run()
{
	process p;

	// jom doesn't handle sigint well, it just continues, so kill it on
	// interruption
	auto pflags = process::terminate_on_interrupt;

	if (flags_ & allow_failure)
	{
		// tasks will set allow_failure for the first couple of runs of jom,
		// which often fails because of the /J multi-process flag, so don't log
		// errors in that case
		p.stderr_level(context::level::trace);
		pflags |= process::allow_failure;
	}

	p
		.binary(binary())
		.cwd(cwd_)
		.stderr_filter([](process::filter& f)
		{
			// initial log line, can't get rid of it, /L or /NOLOGO don't seem
			// to work
			if (f.line.find("empower your cores") != std::string::npos)
				f.lv = context::level::trace;
		})
		.arg("/C", process::log_quiet)  // silent
		.arg("/S", process::log_quiet)  // silent
		.arg("/L", process::log_quiet)  // silent, jom likes to spew crap
		.arg("/D", process::log_dump)   // verbose stuff
		.arg("/P", process::log_dump)	// verbose stuff
		.arg("/W", process::log_dump)	// verbose stuff
		.arg("/K");                     // don't stop on errors

	if (flags_ & single_job)
		p.arg("/J", "1");               // single-process

	for (auto&& def : def_)
		p.arg(def);

	p
		.arg(target_)
		.flags(pflags)
		.env(env::vs(arch_));

	execute_and_join(p);
}

}	// namespace

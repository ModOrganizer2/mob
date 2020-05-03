#include "pch.h"
#include "tools.h"
#include "../conf.h"

namespace mob
{

jom::jom()
	: basic_process_runner("jom"), arch_(arch::def)
{
	process_
		.binary(third_party::jom());
}

jom& jom::path(const fs::path& p)
{
	process_.cwd(p);
	return *this;
}

jom& jom::target(const std::string& s)
{
	target_ = s;
	return *this;
}

jom& jom::def(const std::string& s)
{
	process_.arg(s);
	return *this;
}

jom& jom::flag(flags f)
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
	process::flags_t flags = process::terminate_on_interrupt;
	if (flags_ & allow_failure)
		flags |= process::allow_failure;

	process_
		.set_context(cx_)
		.stderr_filter([](auto&& f)
		{
			if (f.line.find("empower your cores") != std::string::npos)
				f.lv = level::trace;
		})
		.arg("/C", process::quiet)
		.arg("/S", process::quiet)
		.arg("/K");

	if (flags_ & single_job)
		process_.arg("/J", "1");

	process_
		.arg(target_)
		.flags(flags)
		.env(env::vs(arch_));

	execute_and_join();
}

}	// namespace

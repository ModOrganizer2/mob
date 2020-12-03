#include "pch.h"
#include "tools.h"
#include "../core/conf.h"
#include "../core/process.h"

namespace mob
{

basic_process_runner::basic_process_runner(std::string name)
	: tool(std::move(name)), p_(nullptr), code_(0)
{
}

// anchors
basic_process_runner::basic_process_runner(basic_process_runner&& r) = default;
basic_process_runner::~basic_process_runner() = default;

void basic_process_runner::do_interrupt()
{
	if (p_)
		p_->interrupt();
}

int basic_process_runner::execute_and_join(process& p)
{
	// remember the process for do_interrupt()
	p_ = &p;

	// use the process' name for this tool
	set_name(p.name());

	// use this tool's log context for the process
	p.set_context(&cx());

	// run, remember the code because the process object might be destroyed
	code_ = p.run_and_join();

	return code_;
}

int basic_process_runner::exit_code() const
{
	return code_;
}


process_runner::process_runner(process& p)
	: tool(p.name()), p_(p)
{
}

// anchor
process_runner::~process_runner() = default;

void process_runner::do_run()
{
	// use the process' name for this tool
	set_name(p_.name());

	// use this tool's log context for the process
	p_.set_context(&cx());

	// run
	p_.run_and_join();
}

int process_runner::result() const
{
	return p_.exit_code();
}

void process_runner::do_interrupt()
{
	p_.interrupt();
}

}	// namespace

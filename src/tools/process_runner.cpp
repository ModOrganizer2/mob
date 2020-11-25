#include "pch.h"
#include "tools.h"
#include "../core/conf.h"
#include "../core/process.h"

namespace mob
{

basic_process_runner::basic_process_runner(std::string name)
	: tool(std::move(name)), process_(new process)
{
}

basic_process_runner::basic_process_runner(basic_process_runner&& r) = default;

basic_process_runner::~basic_process_runner() = default;

void basic_process_runner::set_process(const process& p)
{
	process_.reset(new process(p));
}

process& basic_process_runner::get_process()
{
	return *process_;
}

void basic_process_runner::do_interrupt()
{
	process_->interrupt();
}

int basic_process_runner::execute_and_join(process& p)
{
	set_name(p.name());
	p.set_context(&cx());
	return p.run_and_join();
}

int basic_process_runner::execute_and_join()
{
	set_name(process_->name());
	process_->set_context(&cx());
	process_->run();
	process_->join();

	return process_->exit_code();
}

int basic_process_runner::exit_code() const
{
	return process_->exit_code();
}


process_runner::process_runner(process& p)
	: tool(p.name()), p_(p)
{
}

process_runner::~process_runner() = default;

void process_runner::do_run()
{
	set_name(p_.name());
	p_.set_context(&cx());

	p_.run();
	p_.join();
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

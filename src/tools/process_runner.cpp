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

int basic_process_runner::execute_and_join()
{
	set_name(process_->name());
	process_->set_context(&cx());
	process_->run();
	join();

	return process_->exit_code();
}

void basic_process_runner::join()
{
	process_->join();
}

int basic_process_runner::exit_code() const
{
	return process_->exit_code();
}


process_runner::process_runner(process&& p)
	: tool(p.name()), own_(new process(std::move(p))), p_(nullptr)
{
}

process_runner::process_runner(process& p)
	: tool(p.name()), p_(&p)
{
}

process_runner::~process_runner() = default;

void process_runner::do_run()
{
	execute_and_join();
}

int process_runner::result() const
{
	return exit_code();
}

void process_runner::do_interrupt()
{
	real_process().interrupt();
}

int process_runner::execute_and_join()
{
	set_name(real_process().name());
	real_process().set_context(&cx());
	real_process().run();

	join();

	return real_process().exit_code();
}

void process_runner::join()
{
	real_process().join();
}

int process_runner::exit_code() const
{
	return real_process().exit_code();
}

process& process_runner::real_process()
{
	if (p_)
		return *p_;
	else
		return *own_;
}

const process& process_runner::real_process() const
{
	if (p_)
		return *p_;
	else
		return *own_;
}

}	// namespace

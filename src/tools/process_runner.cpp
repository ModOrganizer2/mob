#include "pch.h"
#include "tools.h"
#include "../conf.h"

namespace mob
{

basic_process_runner::basic_process_runner(std::string name)
	: tool(std::move(name))
{
}

std::string basic_process_runner::do_name() const
{
	return process_.name();
}

void basic_process_runner::do_interrupt()
{
	process_.interrupt();
}

int basic_process_runner::execute_and_join()
{
	process_.set_context(cx_);
	process_.run();
	join();

	return process_.exit_code();
}

void basic_process_runner::join()
{
	process_.join();
}

int basic_process_runner::exit_code() const
{
	return process_.exit_code();
}


process_runner::process_runner(process&& p)
	: tool(p.name()), own_(std::move(p)), p_(nullptr)
{
}

process_runner::process_runner(process& p)
	: tool(p.name()), p_(&p)
{
}

void process_runner::do_run()
{
	execute_and_join();
}

int process_runner::result() const
{
	return exit_code();
}

std::string process_runner::do_name() const
{
	return real_process().name();
}

void process_runner::do_interrupt()
{
	real_process().interrupt();
}

int process_runner::execute_and_join()
{
	real_process().set_context(cx_);
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
		return own_;
}

const process& process_runner::real_process() const
{
	if (p_)
		return *p_;
	else
		return own_;
}

}	// namespace

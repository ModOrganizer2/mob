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


process_runner::process_runner(process p)
{
	process_ = std::move(p);
}

int process_runner::result() const
{
	return exit_code();
}

void process_runner::do_run()
{
	execute_and_join();
}

}	// namespace

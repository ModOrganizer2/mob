#include "pch.h"
#include "task.h"
#include "../tools.h"
#include "../utility.h"

namespace mob
{

class interrupted {};

std::vector<std::unique_ptr<task>> g_tasks;
std::mutex task::interrupt_mutex_;

void add_task(std::unique_ptr<task> t)
{
	g_tasks.push_back(std::move(t));
}

bool run_task(const std::string& name)
{
	for (auto&& t : g_tasks)
	{
		if (t->name() == name)
		{
			t->run();
			t->join();
			return true;
		}
	}

	std::cerr << "task " << name << " not found\n";
	std::cerr << "valid tasks:\n";

	for (auto&& t : g_tasks)
		std::cerr << " - " << t->name() << "\n";

	return false;
}

void run_all_tasks()
{
	for (auto&& t : g_tasks)
	{
		t->run();
		t->join();
	}
}


task::task(std::string name)
	: name_(std::move(name)), interrupted_(false), tool_(nullptr)
{
}

task::~task()
{
	try
	{
		join();
	}
	catch(bailed)
	{
		// ignore
	}
}

void task::interrupt_all()
{
	std::scoped_lock lock(interrupt_mutex_);
	for (auto&& t : g_tasks)
		t->interrupt();
}


const std::string& task::name() const
{
	return name_;
}

void task::run()
{
	info(name_);

	thread_ = std::thread([&]
	{
		try
		{
			check_interrupted();
			fetch();
			check_interrupted();
			build_and_install();
		}
		catch(bailed e)
		{
			error(name_ + " bailed out, interrupting all tasks");
			interrupt_all();
		}
		catch(interrupted)
		{
			return;
		}
		catch(std::exception& e)
		{
			error(name_ + " uncaught exception: " + e.what());
			interrupt_all();
		}
	});
}

void task::interrupt()
{
	std::scoped_lock lock(tool_mutex_);

	interrupted_ = true;
	if (tool_)
		tool_->interrupt();
}

void task::join()
{
	if (thread_.joinable())
		thread_.join();
}

void task::fetch()
{
	do_fetch();

	run_tool(patcher()
		.task(name_)
		.root(get_source_path()));
}

void task::build_and_install()
{
	do_build_and_install();
}


void task::check_interrupted()
{
	if (interrupted_)
		throw interrupted();
}

}	// namespace

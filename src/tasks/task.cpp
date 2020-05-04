#include "pch.h"
#include "task.h"
#include "../conf.h"
#include "../op.h"
#include "../tools/tools.h"

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
		for (auto&& n : t->names())
		{
			if (n == name)
			{
				t->run();
				t->join();
				return true;
			}
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


task::task(const char* name)
	: task(std::vector<std::string>{name})
{
}

task::task(std::vector<std::string> names)
	: names_(std::move(names)), interrupted_(false), tool_(nullptr)
{
	cx_.task = this;
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
	return names_[0];
}

const std::vector<std::string>& task::names() const
{
	return names_;
}

void task::run()
{
	cx_.info(context::generic, "running task");

	thread_ = std::thread([&]
	{
		try
		{
			if (conf::rebuild())
				clean();

			check_interrupted();
			fetch();
			check_interrupted();
			build_and_install();
		}
		catch(bailed e)
		{
			error(name() + " bailed out, interrupting all tasks");
			interrupt_all();
		}
		catch(interrupted)
		{
			return;
		}
		catch(std::exception& e)
		{
			error(name() + " uncaught exception: " + e.what());
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
	cx_.debug(context::generic, "fetching");
	do_fetch();

	cx_.debug(context::generic, "patching");
	run_tool(patcher()
		.task(name())
		.root(get_source_path()));
}

void task::build_and_install()
{
	cx_.debug(context::generic, "build and install");
	do_build_and_install();
}

void task::clean()
{
	cx_.debug(context::rebuild, "cleaning");
	do_clean();
}

void task::check_interrupted()
{
	if (interrupted_)
		throw interrupted();
}

void task::run_current_tool()
{
	cx_.debug(context::generic, "running tool " + tool_->name());

	check_interrupted();
	tool_->run(cx_);
	check_interrupted();
}

}	// namespace

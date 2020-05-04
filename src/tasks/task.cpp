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
	: names_(std::move(names)), interrupted_(false)
{
	contexts_.push_back(std::make_unique<thread_context>(
		std::this_thread::get_id(), context(name())));
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

const context& task::cx() const
{
	static const context bad("?");

	{
		std::scoped_lock lock(contexts_mutex_);

		for (auto& td : contexts_)
		{
			if (td->tid == std::this_thread::get_id())
				return td->cx;
		}
	}

	return bad;
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

void task::threaded_run(std::string thread_name, std::function<void ()> f)
{
	try
	{
		{
			std::scoped_lock lock(contexts_mutex_);

			contexts_.push_back(std::make_unique<thread_context>(
				std::this_thread::get_id(), context(thread_name)));
		}

		guard g([&]
		{
			std::scoped_lock lock(contexts_mutex_);

			for (auto itor=contexts_.begin(); itor!=contexts_.end(); ++itor)
			{
				if ((*itor)->tid == std::this_thread::get_id())
				{
					contexts_.erase(itor);
					break;
				}
			}
		});

		f();
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
}

void task::run()
{
	cx().info(context::generic, "running task");

	thread_ = std::thread([&]
	{
		threaded_run(name(), [&]
		{
			if (conf::rebuild())
				clean_for_rebuild();

			check_interrupted();
			fetch();
			check_interrupted();
			build_and_install();

			cx().debug(context::generic, "task completed");
		});
	});
}

void task::interrupt()
{
	std::scoped_lock lock(tools_mutex_);

	interrupted_ = true;

	for (auto* t : tools_)
		t->interrupt();
}

void task::join()
{
	if (thread_.joinable())
		thread_.join();
}

void task::fetch()
{
	cx().debug(context::generic, "fetching");
	do_fetch();

	cx().debug(context::generic, "patching");
	run_tool(patcher()
		.task(name())
		.root(get_source_path()));
}

void task::build_and_install()
{
	cx().debug(context::generic, "build and install");
	do_build_and_install();
}

void task::clean_for_rebuild()
{
	cx().debug(context::rebuild, "cleaning");
	do_clean_for_rebuild();
}

void task::check_interrupted()
{
	if (interrupted_)
		throw interrupted();
}

void task::run_tool_impl(tool* t)
{
	{
		std::scoped_lock lock(tools_mutex_);
		tools_.push_back(t);
	}

	guard g([&]
	{
		std::scoped_lock lock(tools_mutex_);

		for (auto itor=tools_.begin(); itor!=tools_.end(); ++itor)
		{
			if (*itor == t)
			{
				tools_.erase(itor);
				break;
			}
		}
	});

	cx().debug(context::generic, "running tool " + t->name());

	context cxcopy(cx());

	check_interrupted();
	t->run(cxcopy);
	check_interrupted();
}

}	// namespace

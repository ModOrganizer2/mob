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

task* find_task(const std::string& name)
{
	for (auto&& t : g_tasks)
	{
		for (auto&& n : t->names())
		{
			if (n == name)
				return t.get();
		}
	}

	std::cerr << "task " << name << " not found\n";
	std::cerr << "valid tasks:\n";

	for (auto&& t : g_tasks)
	{
		std::cerr << " - " << join(t->names(), ", ") << "\n";
	}

	throw bailed("");
}

void run_tasks(const std::vector<task*> tasks)
{
	for (auto* t : tasks)
		t->fetch();

	for (auto* t : tasks)
	{
		t->join();
		t->build_and_install();
		t->join();
	}
}

void run_task(const std::string& name)
{
	run_tasks({find_task(name)});
}

void run_tasks(const std::vector<std::string>& names)
{
	if (names.empty())
		return;

	if (names.size() == 1)
		gcx().debug(context::generic, "specified task: " + names[0]);
	else
		gcx().debug(context::generic, "specified tasks: " + join(names, " "));

	std::vector<task*> tasks;
	for (auto&& name : names)
		tasks.push_back(find_task(name));

	run_tasks(tasks);
}

void run_all_tasks()
{
	std::vector<task*> tasks;
	for (auto&& t : g_tasks)
		tasks.push_back(t.get());

	run_tasks(tasks);
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

	fetch();
	join();

	build_and_install();
	join();

	cx().info(context::generic, "task completed");
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
	thread_ = std::thread([&]
	{
		threaded_run(name(), [&]
		{
			if (conf::rebuild())
				clean_for_rebuild();

			check_interrupted();

			cx().info(context::generic, "fetching");
			do_fetch();

			check_interrupted();

			if (!get_source_path().empty())
			{
				cx().info(context::generic, "patching");
				run_tool(patcher()
					.task(name())
					.root(get_source_path()));
			}

			check_interrupted();
		});
	});
}

void task::build_and_install()
{
	thread_ = std::thread([&]
	{
		threaded_run(name(), [&]
		{
			check_interrupted();

			cx().info(context::generic, "build and install");
			do_build_and_install();

			check_interrupted();
		});
	});
}

void task::clean_for_rebuild()
{
	cx().info(context::rebuild, "cleaning");
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

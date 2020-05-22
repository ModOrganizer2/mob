#include "pch.h"
#include "task.h"
#include "../conf.h"
#include "../op.h"
#include "../tools/tools.h"

namespace mob
{

class interrupted {};

static std::vector<std::unique_ptr<task>> g_tasks;
static std::vector<task*> g_all_tasks;
static std::atomic<bool> g_interrupt = false;
std::mutex task::interrupt_mutex_;

void add_task(std::unique_ptr<task> t)
{
	g_tasks.push_back(std::move(t));
}

const std::vector<task*>& get_all_tasks()
{
	return g_all_tasks;
}

void list_tasks(bool err)
{
	for (auto&& t : g_all_tasks)
	{
		if (err)
			u8cerr << " - " << join(t->names(), ", ") << "\n";
		else
			u8cout << " - " << join(t->names(), ", ") << "\n";
	}
}

task* find_task(const std::string& name)
{
	for (auto&& t : g_all_tasks)
	{
		for (auto&& n : t->names())
		{
			if (n == name)
				return t;
		}
	}

	u8cout << "task " << name << " not found\n";
	u8cout << "valid tasks:\n";
	list_tasks(true);

	throw bailed("");
}

void run_tasks(const std::vector<task*>& tasks)
{
	auto task_comp = [](task* t1, task* t2) {
		return std::find(std::begin(g_all_tasks), std::end(g_all_tasks), t1) 
			< std::find(std::begin(g_all_tasks), std::end(g_all_tasks), t2);
	};
	try
	{
		{
			std::set<task*, decltype(task_comp)> set(tasks.begin(), tasks.end(), task_comp);
			MOB_ASSERT(set.size() == tasks.size());
		}

		for (auto* t : tasks)
		{
			t->fetch();

			if (g_interrupt)
				throw interrupted();
		}

		for (auto* t : tasks)
		{
			t->join();

			if (g_interrupt)
				throw interrupted();

			t->build_and_install();

			if (g_interrupt)
				throw interrupted();

			t->join();

			if (g_interrupt)
				throw interrupted();
		}
	}
	catch(interrupted&)
	{
	}
}

void gather_super_tasks(std::vector<task*>& tasks, std::set<task*>& seen)
{
	for (auto& t : g_tasks)
	{
		if (t->is_super())
		{
			if (!seen.contains(t.get()))
			{
				tasks.push_back(t.get());
				seen.insert(t.get());
			}
		}
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
		gcx().debug(context::generic, "specified task: {}", names[0]);
	else
		gcx().debug(context::generic, "specified tasks: {}", join(names, " "));

	std::vector<task*> tasks;
	std::set<task*> seen;
	for (auto&& name : names)
	{
		if (name == "super")
		{
			gather_super_tasks(tasks, seen);
		}
		else
		{
			auto* t = find_task(name);

			if (!seen.contains(t))
			{
				tasks.push_back(t);
				seen.insert(t);
			}
		}
	}

	run_tasks(tasks);
}

void run_all_tasks()
{
	std::vector<task*> tasks;

	for (auto&& t : g_tasks)
		tasks.push_back(t.get());

	run_tasks(tasks);
}

bool task_exists(const std::string& name)
{
	if (name == "super")
		return true;

	for (auto&& t : g_all_tasks)
	{
		for (auto&& n : t->names())
		{
			if (n == name)
				return true;
		}
	}

	return false;
}

bool is_super_task(const std::string& name)
{
	for (auto& t : g_all_tasks)
	{
		for (auto&& tn : t->names())
		{
			if (tn == name)
				return t->is_super();
		}
	}

	return false;
}


struct task::thread_context
{
	std::thread::id tid;
	context cx;

	thread_context(std::thread::id tid, context cx)
		: tid(tid), cx(std::move(cx))
	{
	}
};


task_conf_holder::task_conf_holder(const task& t)
	: task_(t)
{
}

std::string task_conf_holder::mo_org() const
{
	return conf::option_by_name(task_.names(), "mo_org");
}

std::string task_conf_holder::mo_branch() const
{
	return conf::option_by_name(task_.names(), "mo_branch");
}

bool task_conf_holder::no_pull() const
{
	return conf::bool_option_by_name(task_.names(), "no_pull");
}

bool task_conf_holder::revert_ts() const
{
	return conf::bool_option_by_name(task_.names(), "revert_ts");
}

bool task_conf_holder::ignore_ts()const
{
	return conf::bool_option_by_name(task_.names(), "ignore_ts");
}

std::string task_conf_holder::git_url_prefix() const
{
	return conf::option_by_name(task_.names(), "git_url_prefix");
}

bool task_conf_holder::git_shallow() const
{
	return conf::bool_option_by_name(task_.names(), "git_shallow");
}

std::string task_conf_holder::git_user() const
{
	return conf::option_by_name(task_.names(), "git_username");
}

std::string task_conf_holder::git_email() const
{
	return conf::option_by_name(task_.names(), "git_email");
}

bool task_conf_holder::set_origin_remote() const
{
	return conf::bool_option_by_name(task_.names(), "set_origin_remote");
}

std::string task_conf_holder::remote_org() const
{
	return conf::option_by_name(task_.names(), "remote_org");
}

std::string task_conf_holder::remote_key() const
{
	return conf::option_by_name(task_.names(), "remote_key");
}

bool task_conf_holder::remote_no_push_upstream() const
{
	return conf::bool_option_by_name(task_.names(), "remote_no_push_upstream");
}

bool task_conf_holder::remote_push_default_origin() const
{
	return conf::bool_option_by_name(task_.names(), "remote_push_default_origin");
}

git task_conf_holder::make_git(git::ops o) const
{
	if (o == git::ops::none)
	{
		if (no_pull())
			o = git::ops::clone;
		else
			o = git::ops::clone_or_pull;
	}

	git g(o);

	g.ignore_ts_on_clone(ignore_ts());
	g.revert_ts_on_pull(revert_ts());
	g.credentials(git_user(), git_email());
	g.shallow(git_shallow());

	if (set_origin_remote())
	{
		g.remote(
			remote_org(), remote_key(),
			remote_no_push_upstream(),
			remote_push_default_origin());
	}

	return g;
}

std::string task_conf_holder::make_git_url(
	const std::string& org, const std::string& repo) const
{
	return git_url_prefix() + org + "/" + repo + ".git";
}


std::array<std::string, 7> time_names()
{
	return {
		"init_super",
		"fetch",
		"extract",
		"configure",
		"build",
		"install",
		"clean"
	};
}


task::task(std::vector<std::string> names) :
	instrumentable(names[0], time_names()),
	names_(std::move(names)), interrupted_(false)
{
	contexts_.push_back(std::make_unique<thread_context>(
		std::this_thread::get_id(), context(name())));

	if (name() != "parallel")
		g_all_tasks.push_back(this);
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

bool task::is_super() const
{
	return false;
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

void task::add_name(std::string s)
{
	names_.push_back(s);
}

void task::interrupt_all()
{
	std::scoped_lock lock(interrupt_mutex_);

	g_interrupt = true;
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
		error("{} bailed out, interrupting all tasks", name());
		interrupt_all();
	}
	catch(interrupted)
	{
		return;
	}
}

void task::parallel(std::vector<std::pair<std::string, std::function<void ()>>> v)
{
	std::vector<std::thread> ts;

	for (auto&& [name, f] : v)
	{
		cx().trace(context::generic, "running in parallel: {}", name);

		ts.push_back(start_thread([this, name, f]
			{
				threaded_run(name, f);
			}));
	}

	for (auto&& t : ts)
		t.join();
}

task_conf_holder task::task_conf() const
{
	return task_conf_holder(*this);
}

void task::run()
{
	threaded_run(name(), [&]
	{
		cx().info(context::generic, "running task");

		fetch();
		join();

		check_interrupted();

		build_and_install();
		join();

		check_interrupted();
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
	thread_ = start_thread([&]
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
				cx().debug(context::generic, "patching");

				run_tool(patcher()
					.task(name(), get_prebuilt())
					.root(get_source_path()));
			}

			check_interrupted();
		});
	});
}

void task::build_and_install()
{
	thread_ = start_thread([&]
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

	cx().debug(context::generic, "running tool {}", t->name());

	context cxcopy(cx());

	check_interrupted();
	t->run(cxcopy);
	check_interrupted();
}


parallel_tasks::parallel_tasks(bool super)
	: task("parallel"), super_(super)
{
}

bool parallel_tasks::is_super() const
{
	return super_;
}

void parallel_tasks::run()
{
	for (auto& t : children_)
	{
		threads_.push_back(start_thread([&]
		{
			t->run();
		}));
	}

	join();
}


void parallel_tasks::interrupt()
{
	for (auto& t : children_)
		t->interrupt();
}

void parallel_tasks::join()
{
	for (auto& t : threads_)
		t.join();

	threads_.clear();
}

void parallel_tasks::fetch()
{
	// no-op
}

void parallel_tasks::build_and_install()
{
	threaded_run(name(), [&]
	{
		for (auto& t : children_)
		{
			threads_.push_back(start_thread([&]
			{
				t->run();
			}));
		}
	});
}

void parallel_tasks::do_fetch()
{
}

void parallel_tasks::do_build_and_install()
{
}

void parallel_tasks::do_clean_for_rebuild()
{
}

}	// namespace

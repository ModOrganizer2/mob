#include "pch.h"
#include "task.h"
#include "../core/conf.h"
#include "../core/op.h"
#include "../tools/tools.h"
#include "../utility/threading.h"

namespace mob
{

class interrupted {};

static std::vector<std::unique_ptr<task>> g_tasks;
static std::vector<task*> g_all_tasks;
static std::atomic<bool> g_interrupt = false;
static alias_map g_aliases;
std::mutex task::interrupt_mutex_;


void add_task(std::unique_ptr<task> t)
{
	g_tasks.push_back(std::move(t));
}

std::vector<task*> get_all_tasks()
{
	std::vector<task*> v;

	for (auto&& t : g_all_tasks)
		v.push_back(t);

	return v;
}

std::vector<task*> get_top_level_tasks()
{
	std::vector<task*> v;

	for (auto&& t : g_tasks)
		v.push_back(t.get());

	return v;
}

std::vector<task*> find_tasks_by_pattern(std::string_view pattern)
{
	std::vector<task*> tasks;

	for (auto&& t : g_all_tasks)
	{
		if (pattern == "super" && t->is_super())
		{
			tasks.push_back(t);
		}
		else
		{
			for (auto&& n : t->names())
			{
				if (mob::glob_match(pattern, n))
				{
					tasks.push_back(t);
					break;
				}
			}
		}
	}

	return tasks;
}

std::vector<task*> find_tasks_by_alias(std::string_view pattern)
{
	std::vector<task*> v;

	auto itor = g_aliases.find(pattern);
	if (itor == g_aliases.end())
		return v;

	for (auto&& a : itor->second)
	{
		const auto temp = find_tasks_by_pattern(a);
		v.insert(v.end(), temp.begin(), temp.end());
	}

	return v;
}

std::vector<task*> find_tasks(std::string_view pattern)
{
	std::vector<task*> tasks;

	for (auto&& t : g_all_tasks)
	{
		if (pattern == "super" && t->is_super())
		{
			tasks.push_back(t);
		}
		else
		{
			for (auto&& n : t->names())
			{
				if (mob::glob_match(pattern, n))
				{
					tasks.push_back(t);
					break;
				}
			}
		}
	}

	if (tasks.empty())
		tasks = find_tasks_by_alias(pattern);

	return tasks;
}

task* find_one_task(std::string_view pattern, bool verbose)
{
	const auto tasks = find_tasks(pattern);

	if (tasks.empty())
	{
		if (verbose)
			u8cerr << "no task matches '" << pattern << "'\n";

		return nullptr;
	}
	else if (tasks.size() > 1)
	{
		if (verbose)
		{
			u8cerr
				<< "found " << tasks.size() << " matches for pattern "
				<< "'" << pattern << "'\n"
				<< "the pattern must only match one task\n";
		}

		return nullptr;
	}

	return tasks[0];
}

bool valid_task_name(std::string_view pattern)
{
	if (!find_tasks(pattern).empty())
		return true;

	if (pattern == "_override")
		return true;

	return false;
}

void run_all_tasks()
{
	try
	{
		for (auto& t : g_tasks)
		{
			t->fetch();

			if (g_interrupt)
				throw interrupted();
		}

		for (auto& t : g_tasks)
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

void add_alias(std::string name, std::vector<std::string> names)
{
	auto itor = g_aliases.find(name);
	if (itor != g_aliases.end())
	{
		gcx().warning(context::generic, "alias {} already exists", name);
		return;
	}

	g_aliases.emplace(std::move(name), std::move(names));
}

const alias_map& get_all_aliases()
{
	return g_aliases;
}



std::string to_string(task::clean c)
{
	// generate warnings if something is added
	switch (c)
	{
		case task::clean::nothing: break;
		case task::clean::redownload: break;
		case task::clean::reextract: break;
		case task::clean::reconfigure: break;
		case task::clean::rebuild: break;
		case task::clean::everything: break;
	}

	std::vector<std::string> v;

	if (is_set(c, task::clean::redownload))
		v.push_back("redownload");

	if (is_set(c, task::clean::reextract))
		v.push_back("reextract");

	if (is_set(c, task::clean::reconfigure))
		v.push_back("reconfigure");

	if (is_set(c, task::clean::rebuild))
		v.push_back("rebuild");

	return join(v, "|");
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
	return conf().task(task_.names()).get("mo_org");
}

std::string task_conf_holder::mo_branch() const
{
	return conf().task(task_.names()).get("mo_branch");
}

bool task_conf_holder::no_pull() const
{
	return conf().task(task_.names()).get<bool>("no_pull");
}

bool task_conf_holder::revert_ts() const
{
	return conf().task(task_.names()).get<bool>("revert_ts");
}

bool task_conf_holder::ignore_ts()const
{
	return conf().task(task_.names()).get<bool>("ignore_ts");
}

std::string task_conf_holder::git_url_prefix() const
{
	return conf().task(task_.names()).get("git_url_prefix");
}

bool task_conf_holder::git_shallow() const
{
	return conf().task(task_.names()).get<bool>("git_shallow");
}

std::string task_conf_holder::git_user() const
{
	return conf().task(task_.names()).get("git_username");
}

std::string task_conf_holder::git_email() const
{
	return conf().task(task_.names()).get("git_email");
}

bool task_conf_holder::set_origin_remote() const
{
	return conf().task(task_.names()).get<bool>("set_origin_remote");
}

std::string task_conf_holder::remote_org() const
{
	return conf().task(task_.names()).get("remote_org");
}

std::string task_conf_holder::remote_key() const
{
	return conf().task(task_.names()).get("remote_key");
}

bool task_conf_holder::remote_no_push_upstream() const
{
	return conf().task(task_.names()).get<bool>("remote_no_push_upstream");
}

bool task_conf_holder::remote_push_default_origin() const
{
	return conf().task(task_.names()).get<bool>("remote_push_default_origin");
}

git task_conf_holder::make_git(git::ops o) const
{
	if (o == git::clone_or_pull && no_pull())
		o = git::clone;

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


task::task(std::vector<std::string> names)
	: names_(std::move(names)), interrupted_(false)
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

bool task::enabled() const
{
	return conf().task(names()).get<bool>("enabled");
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
	auto itor = std::find(names_.begin(), names_.end(), s);
	if (itor != names_.end())
		return;

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
		gcx().error(context::generic,
			"{} bailed out, interrupting all tasks", name());

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
		if (!enabled())
		{
			cx().debug(context::generic, "task is disabled");
			return;
		}

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

void task::clean_task()
{
	if (!conf().global().clean())
		return;

	if (!enabled())
	{
		cx().debug(context::generic, "cleaning (skipping, task disabled)");
		return;
	}


	const auto cf = make_clean_flags();

	if (cf != clean::nothing)
	{
		cx().info(context::rebuild, "cleaning ({})", to_string(cf));
		do_clean(cf);
	}
}

void task::fetch()
{
	if (!enabled())
	{
		cx().debug(context::generic, "fetching (skipping, task disabled)");
		return;
	}

	thread_ = start_thread([&]
	{
		threaded_run(name(), [&]
		{
			clean_task();
			check_interrupted();

			if (conf().global().fetch())
			{
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
			}
		});
	});
}

void task::build_and_install()
{
	if (!conf().global().build())
		return;

	if (!enabled())
	{
		cx().debug(context::generic,
			"build and install (skipping, task disabled)");
		return;
	}

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

task::clean task::make_clean_flags() const
{
	clean c = clean::nothing;
	const auto g = conf().global();

	if (g.redownload())
		c |= clean::redownload;

	if (g.reextract())
		c |= clean::reextract;

	if (g.reconfigure())
		c |= clean::reconfigure;

	if (g.rebuild())
		c |= clean::rebuild;

	return c;
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


parallel_tasks::parallel_tasks()
	: container_task("parallel")
{
}

void parallel_tasks::add_task(std::unique_ptr<task> t)
{
	if (!children_.empty() && children_[0]->is_super() != t->is_super())
	{
		gcx().bail_out(context::generic,
			"parallel task can't mix super and non-super tasks");
	}

	children_.push_back(std::move(t));
}

std::vector<task*> parallel_tasks::children() const
{
	std::vector<task*> v;

	for (auto&& t : children_)
		v.push_back(t.get());

	return v;
}

bool parallel_tasks::is_super() const
{
	if (children_.empty())
		return false;

	return children_[0]->is_super();
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

void parallel_tasks::do_clean(clean)
{
	for (auto& t : children_)
	{
		threads_.push_back(start_thread([&]
		{
			t->clean_task();
		}));
	}

	join();
}

}	// namespace

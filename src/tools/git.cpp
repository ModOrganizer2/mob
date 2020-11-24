#include "pch.h"
#include "tools.h"
#include "../core/conf.h"
#include "../core/process.h"
#include "../utility/threading.h"

namespace mob
{

constexpr git::ops no_op(git::ops(0));
const std::string default_github_url_pattern = "git@github.com:{}/{}";


[[nodiscard]] process make_process()
{
	static env e = this_env::get()
		.set("GCM_INTERACTIVE", "never")
		.set("GIT_TERMINAL_PROMPT", "0");

	return std::move(process()
		.binary(git::binary())
		.env(e));
}

[[nodiscard]] process init(const fs::path& root)
{
	return make_process()
		.arg("init")
		.cwd(root);
}

[[nodiscard]] process set_config(
	const fs::path& root, const std::string& key, const std::string& value)
{
	return make_process()
		.stderr_level(context::level::trace)
		.arg("config")
		.arg(key)
		.arg(value)
		.cwd(root);
}

[[nodiscard]] process apply(const fs::path& root, const std::string& diff)
{
	return make_process()
		.stdin_string(diff)
		.arg("apply")
		.arg("--whitespace", "nowarn")
		.arg("-")
		.cwd(root);
}

[[nodiscard]] process fetch(
	const fs::path& root, const std::string& remote, const std::string& branch)
{
	return make_process()
		.arg("fetch")
		.arg("-q")
		.arg(remote)
		.arg(branch)
		.cwd(root);
}

[[nodiscard]] process checkout(const fs::path& root, const std::string& what)
{
	return make_process()
		.arg("-c", "advice.detachedHead=false")
		.arg("checkout")
		.arg("-q")
		.arg(what)
		.cwd(root);
}

[[nodiscard]] process revert(const fs::path& root, const fs::path& file)
{
	return make_process()
		.stderr_level(context::level::trace)
		.arg("checkout")
		.arg(file)
		.cwd(root);
}

[[nodiscard]] process current_branch(const fs::path& root)
{
	return make_process()
		.stdout_flags(process::keep_in_string)
		.arg("branch")
		.arg("--show-current")
		.cwd(root);
}

[[nodiscard]] process add_submodule(
	const fs::path& root,
	const std::string& branch, const std::string& submodule,
	const mob::url& url)
{
	return make_process()
		.stderr_level(context::level::trace)
		.arg("-c", "core.autocrlf=false")
		.arg("submodule")
		.arg("--quiet")
		.arg("add")
		.arg("-b", branch)
		.arg("--force")
		.arg("--name", submodule)
		.arg(url)
		.arg(submodule)
		.cwd(root);
}

[[nodiscard]] process clone(
	const fs::path& root, const mob::url& url, const std::string& branch,
	bool shallow)
{
	auto p = make_process()
		.stderr_level(context::level::trace)
		.arg("clone")
		.arg("--recurse-submodules");

	if (shallow)
		p.arg("--depth", "1");

	p
		.arg("--branch", branch)
		.arg("--quiet", process::log_quiet)
		.arg("-c", "advice.detachedHead=false", process::log_quiet)
		.arg(url)
		.arg(root);

	return p;
}

[[nodiscard]] process pull(
	const fs::path& root, const mob::url& url, const std::string& branch)
{
	return make_process()
		.stderr_level(context::level::trace)
		.arg("pull")
		.arg("--recurse-submodules")
		.arg("--quiet", process::log_quiet)
		.arg(url)
		.arg(branch)
		.cwd(root);
}

[[nodiscard]] process has_remote(const fs::path& root, const std::string& name)
{
	return make_process()
		.flags(process::allow_failure)
		.stderr_level(context::level::debug)
		.arg("config")
		.arg("remote." + name + ".url")
		.cwd(root);
}

[[nodiscard]] process rename_remote(
	const fs::path& root, const std::string& from, const std::string& to)
{
	return make_process()
		.arg("remote")
		.arg("rename")
		.arg(from)
		.arg(to)
		.cwd(root);
}

[[nodiscard]] process add_remote(
	const fs::path& root, const std::string& name, const std::string& url)
{
	return make_process()
		.arg("remote")
		.arg("add")
		.arg(name)
		.arg(url)
		.cwd(root);
}

[[nodiscard]] process set_remote_push(
	const fs::path& root, const std::string& remote, const std::string& url)
{
	return make_process()
		.arg("remote")
		.arg("set-url")
		.arg("--push")
		.arg(remote)
		.arg(url)
		.cwd(root);
}

[[nodiscard]] process set_assume_unchanged(
	const fs::path& root, const fs::path& file, bool on)
{
	return make_process()
		.arg("update-index")
		.arg(on ? "--assume-unchanged" : "--no-assume-unchanged")
		.arg(file, process::forward_slashes)
		.cwd(root);
}

[[nodiscard]] process is_tracked(const fs::path& root, const fs::path& file)
{
	return make_process()
		.stdout_level(context::level::debug)
		.stderr_level(context::level::debug)
		.flags(process::allow_failure)
		.arg("ls-files")
		.arg("--error-unmatch")
		.arg(file, process::forward_slashes)
		.cwd(root);
}

[[nodiscard]] process is_repo(const fs::path& root)
{
	return make_process()
		.arg("rev-parse")
		.arg("--is-inside-work-tree")
		.stderr_filter([](process::filter& f)
		{
			if (f.line.find("not a git repo") != std::string::npos)
				f.lv = context::level::trace;
		})
		.flags(process::allow_failure)
		.cwd(root);
}

[[nodiscard]] process remote_branch_exists(
	const mob::url& url, const std::string& branch)
{
	return make_process()
		.flags(process::allow_failure)
		.arg("ls-remote")
		.arg("--exit-code")
		.arg("--heads")
		.arg(url)
		.arg(branch);
}

[[nodiscard]] process has_uncommitted_changes(const fs::path& root)
{
	return make_process()
		.flags(process::allow_failure)
		.stdout_flags(process::keep_in_string)
		.arg("status")
		.arg("-s")
		.arg("--porcelain")
		.cwd(root);
}

[[nodiscard]] process has_stashed_changes(const fs::path& root)
{
	return make_process()
		.flags(process::allow_failure)
		.stderr_level(context::level::trace)
		.arg("stash show")
		.cwd(root);
}

[[nodiscard]] process git_file(const fs::path& root)
{
	return make_process()
		.stdout_flags(process::keep_in_string)
		.arg("remote")
		.arg("get-url")
		.arg("origin")
		.cwd(root);
}


git::git(ops o)
	: basic_process_runner("git"), op_(o)
{
}

void git::delete_directory(const context& cx, const fs::path& p)
{
	git g(no_op);
	g.root(p);

	if (!conf().global().get<bool>("ignore_uncommitted"))
	{
		if (g.has_uncommitted_changes())
		{
			cx.bail_out(context::redownload,
				"will not delete {}, has uncommitted changes; "
				"see --ignore-uncommitted-changes", p);
		}

		if (g.has_stashed_changes())
		{
			cx.bail_out(context::redownload,
				"will not delete {}, has stashed changes; "
				"see --ignore-uncommitted-changes", p);
		}
	}

	cx.trace(context::redownload, "deleting directory controlled by git{}", p);
	op::delete_directory(cx, p, op::optional);
}

void git::set_credentials(
	const fs::path& repo,
	const std::string& username, const std::string& email)
{
	git(no_op)
		.root(repo)
		.credentials(username, email)
		.do_set_credentials();
}

void git::set_remote(
	const fs::path& repo,
	std::string org, std::string key,
	bool no_push_upstream, bool push_default_origin)
{
	git(no_op)
		.root(repo)
		.remote(org, key, no_push_upstream, push_default_origin)
		.do_set_remote();
}

void git::ignore_ts(const fs::path& repo, bool b)
{
	git(no_op)
		.root(repo)
		.ignore_ts_on_clone(b)
		.do_ignore_ts();
}

bool git::has_remote(const fs::path& repo, const std::string& name)
{
	git g(no_op);
	g.root(repo);
	return g.has_remote(name);
}

void git::add_remote(
	const fs::path& repo, const std::string& remote_name,
	const std::string& username, const std::string& key, bool push_default,
	const std::string& url_pattern)
{
	git g(no_op);
	g.root(repo);

	const auto gf = g.git_file();

	if (!g.has_remote(remote_name))
	{
		g.add_remote(remote_name, make_url(username, gf, url_pattern));

		if (push_default)
			g.set_config("remote.pushdefault", remote_name);

		if (!key.empty())
			g.set_config("remote." + remote_name + ".puttykeyfile", key);
	}
}

bool git::is_git_repo(const fs::path& p)
{
	git g(no_op);
	g.root(p);
	return g.is_repo();
}

bool git::remote_branch_exists(const mob::url& u, const std::string& name)
{
	git g(no_op);
	g.url(u);
	g.branch(name);
	return g.remote_branch_exists();
}

void git::init_repo(const fs::path& p)
{
	git g(no_op);
	g.root(p);
	g.init();
}

void git::apply(const fs::path& p, const std::string& diff)
{
	mob::apply(p, diff).run_and_join();
}

void git::fetch(
	const fs::path& p, const std::string& remote, const std::string& branch)
{
	mob::fetch(p, remote, branch).run_and_join();
}

void git::checkout(const fs::path& p, const std::string& what)
{
	mob::checkout(p, what).run_and_join();
}

void git::checkout(const std::string& what)
{
	execute_and_join(mob::checkout(root_, what));
}

std::string git::current_branch(const fs::path& root)
{
	auto p = mob::current_branch(root);
	p.run_and_join();
	return trim_copy(p.stdout_string());
}

fs::path git::binary()
{
	return conf().tool().get("git");
}

git& git::url(const mob::url& u)
{
	url_ = u;
	return *this;
}

git& git::branch(const std::string& name)
{
	branch_ = name;
	return *this;
}

git& git::root(const fs::path& dir)
{
	root_ = dir;
	return *this;
}

const fs::path& git::root() const
{
	return root_;
}

git& git::credentials(const std::string& username, const std::string& email)
{
	creds_username_ = username;
	creds_email_ = email;
	return *this;
}

git& git::submodule_name(const std::string& name)
{
	submodule_ = name;
	return *this;
}

const std::string& git::submodule_name() const
{
	return submodule_;
}

git& git::remote(
	std::string org, std::string key,
	bool no_push_upstream, bool push_default_origin)
{
	remote_org_ = org;
	remote_key_ = key;
	no_push_upstream_ = no_push_upstream;
	push_default_origin_ = push_default_origin;

	return *this;
}

git& git::ignore_ts_on_clone(bool b)
{
	ignore_ts_ = b;
	return *this;
}

git& git::revert_ts_on_pull(bool b)
{
	revert_ts_ = b;
	return *this;
}

git& git::shallow(bool b)
{
	shallow_ = b;
	return *this;
}

void git::do_run()
{
	if (url_.empty() || root_.empty())
		cx().bail_out(context::generic, "git missing parameters");


	switch (op_)
	{
		case ops::clone:
		{
			do_clone();
			break;
		}

		case ops::pull:
		{
			do_pull();
			break;
		}

		case ops::clone_or_pull:
		{
			do_clone_or_pull();
			break;
		}

		case ops::add_submodule:
		{
			do_add_submodule();
			break;
		}

		default:
		{
			cx().bail_out(context::generic, "git unknown op {}", op_);
		}
	}
}

void git::do_add_submodule()
{
	execute_and_join(mob::add_submodule(root_, branch_, submodule_, url_));
}

void git::do_clone_or_pull()
{
	if (!do_clone())
		do_pull();
}

bool git::do_clone()
{
	const fs::path dot_git = root_ / ".git";
	if (fs::exists(dot_git))
	{
		cx().trace(context::generic, "not cloning, {} exists", dot_git);
		return false;
	}

	execute_and_join(mob::clone(root_, url_, branch_, shallow_));

	if (!creds_username_.empty() || !creds_email_.empty())
		do_set_credentials();

	if (!remote_org_.empty())
		do_set_remote();

	if (ignore_ts_)
		do_ignore_ts();

	return true;
}

void git::do_pull()
{
	if (revert_ts_)
		do_revert_ts();

	execute_and_join(mob::pull(root_, url_, branch_));
}

void git::do_set_credentials()
{
	cx().debug(context::generic, "setting up credentials");

	if (!creds_username_.empty())
		set_config("user.name", creds_username_);

	if (!creds_email_.empty())
		set_config("user.email", creds_email_);
}

void git::do_set_remote()
{
	if (has_remote("upstream"))
	{
		cx().trace(context::generic, "upstream remote already exists");
		return;
	}

	const auto gf = git_file();

	rename_remote("origin", "upstream");

	if (no_push_upstream_)
		set_remote_push("upstream", "nopushurl");

	add_remote("origin", make_url(remote_org_, gf));

	if (push_default_origin_)
		set_config("remote.pushdefault", "origin");

	if (!remote_key_.empty())
		set_config("remote.origin.puttykeyfile", remote_key_);
}

template <class F>
void for_each_ts(const fs::path& root, F&& f)
{
	for (auto&& e : fs::recursive_directory_iterator(root))
	{
		if (!e.is_regular_file())
			continue;

		const auto p = e.path();

		if (!path_to_utf8(p.extension()).ends_with(".ts"))
			continue;

		f(p);
	}
}

void git::do_ignore_ts()
{
	for_each_ts(root_, [&](auto&& p)
	{
		const auto rp = fs::relative(p, root_);

		if (is_tracked(rp))
		{
			cx().trace(context::generic, "  . {}", rp);
			set_assume_unchanged(rp, true);
		}
		else
		{
			cx().trace(context::generic, "  . {} (skipping, not tracked)", rp);
		}
	});
}

void git::do_revert_ts()
{
	for_each_ts(root_, [&](auto&& p)
	{
		const auto rp = fs::relative(p, root_);

		if (!is_tracked(rp))
		{
			cx().debug(context::generic,
				"won't try to revert ts file '{}', not tracked", rp);

			return;
		}

		execute_and_join(mob::revert(root_, p));
	});
}

void git::set_config(const std::string& key, const std::string& value)
{
	execute_and_join(mob::set_config(root_, key, value));
}

bool git::has_remote(const std::string& name)
{
	return (execute_and_join(mob::has_remote(root_, name)) == 0);
}

void git::rename_remote(const std::string& from, const std::string& to)
{
	execute_and_join(mob::rename_remote(root_, from, to));
}

void git::add_remote(const std::string& name, const std::string& url)
{
	execute_and_join(mob::add_remote(root_, name, url));
}

void git::set_remote_push(const std::string& remote, const std::string& url)
{
	execute_and_join(mob::set_remote_push(root_, remote, url));
}

void git::set_assume_unchanged(const fs::path& file, bool on)
{
	execute_and_join(mob::set_assume_unchanged(root_, file, on));
}

bool git::is_tracked(const fs::path& file)
{
	return (execute_and_join(mob::is_tracked(root_, file)) == 0);
}

bool git::is_repo()
{
	return (execute_and_join(mob::is_repo(root_)) == 0);
}

bool git::remote_branch_exists()
{
	return (execute_and_join(mob::remote_branch_exists(url_, branch_)) == 0);
}

bool git::has_uncommitted_changes()
{
	execute_and_join(mob::has_uncommitted_changes(root_));
	return (get_process().stdout_string() != "");
}

bool git::has_stashed_changes()
{
	return (execute_and_join(mob::has_stashed_changes(root_)) == 0);
}

void git::init()
{
	execute_and_join(mob::init(root_));
}

std::string git::git_file()
{
	execute_and_join(mob::git_file(root_));
	const std::string out = get_process().stdout_string();

	const auto last_slash = out.find_last_of("/");
	if (last_slash == std::string::npos)
	{
		u8cerr << "bad get-url output '" << out << "'\n";
		throw bailed();
	}

	auto s = trim_copy(out.substr(last_slash + 1));

	if (s.empty())
	{
		u8cerr << "bad get-url output '" << out << "'\n";
		throw bailed();
	}

	return s;
}

std::string git::make_url(
	const std::string& org, const std::string& git_file,
	const std::string& url_pattern)
	{
	const std::string pattern =
		url_pattern.empty() ? default_github_url_pattern : url_pattern;

	return fmt::format(pattern, org, git_file);
}



static std::unique_ptr<git_submodule_adder> g_sa_instance;
static std::mutex g_sa_instance_mutex;

git_submodule_adder::git_submodule_adder() :
	instrumentable("submodule_adder",
		{"add_submodule_wait", "add_submodule"}),
	cx_("submodule_adder"), quit_(false)
{
	run();
}

git_submodule_adder::~git_submodule_adder()
{
	stop();

	if (thread_.joinable())
		thread_.join();
}

git_submodule_adder& git_submodule_adder::instance()
{
	std::scoped_lock lock(g_sa_instance_mutex);
	if (!g_sa_instance)
		g_sa_instance.reset(new git_submodule_adder);

	return *g_sa_instance;
}

void git_submodule_adder::queue(git g)
{
	std::scoped_lock lock(queue_mutex_);
	queue_.emplace_back(std::move(g));
	wakeup();
}

void git_submodule_adder::run()
{
	thread_ = start_thread([&]{ thread_fun(); });
}

void git_submodule_adder::stop()
{
	quit_ = true;
	wakeup();
}

void git_submodule_adder::thread_fun()
{
	try
	{
		while (!quit_)
		{
			instrument<times::add_submodule_wait>([&]
			{
				std::unique_lock lk(sleeper_.m);
				sleeper_.cv.wait(lk, [&]{ return sleeper_.ready; });
				sleeper_.ready = false;
			});

			if (quit_)
				break;

			process();
		}
	}
	catch(bailed&)
	{
		// silent
		return;
	}
}

void git_submodule_adder::wakeup()
{
	{
		std::scoped_lock lk(sleeper_.m);
		sleeper_.ready = true;
	}

	sleeper_.cv.notify_one();
}

void git_submodule_adder::process()
{
	std::vector<git> v;

	{
		std::scoped_lock lock(queue_mutex_);
		v.swap(queue_);
	}

	cx_.trace(context::generic,
		"git_submodule_adder: woke up, {} to process", v.size());

	for (auto&& g : v)
	{
		instrument<times::add_submodule>([&]
		{
			cx_.trace(context::generic,
				"git_submodule_adder: running {}", g.submodule_name());

			g.run(cx_);
		});

		if (quit_)
			break;
	}
}

}	// namespace

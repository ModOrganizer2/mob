#include "pch.h"
#include "tools.h"
#include "../core/conf.h"
#include "../core/process.h"
#include "../utility/threading.h"

namespace mob::details
{

const std::string default_github_url_pattern = "git@github.com:{}/{}";

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


[[nodiscard]] process make_process()
{
	static env e = this_env::get()
		.set("GCM_INTERACTIVE", "never")
		.set("GIT_TERMINAL_PROMPT", "0");

	return std::move(process()
		.binary(git_tool::binary())
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

}	// namespace


namespace mob
{

git_tool::git_tool(fs::path root, basic_process_runner* runner)
	: root_(std::move(root)), runner_(runner)
{
}

fs::path git_tool::binary()
{
	return conf().tool().get("git");
}

int git_tool::run(process&& p)
{
	if (runner_)
		return runner_->execute_and_join(p);
	else
		return p.run_and_join();
}

int git_tool::run(process& p)
{
	if (runner_)
		return runner_->execute_and_join(p);
	else
		return p.run_and_join();
}

const context& git_tool::cx()
{
	if (runner_)
		return runner_->cx();
	else
		return gcx();
}

void git_tool::clone(const mob::url& url, const std::string& branch, bool shallow)
{
	run(details::clone(root_, url, branch, shallow));
}

void git_tool::pull(const mob::url& url, const std::string& branch)
{
	run(details::pull(root_, url, branch));
}

void git_tool::set_credentials(const std::string& username, const std::string& email)
{
	cx().debug(context::generic, "setting up credentials");

	if (!username.empty())
		set_config("user.name", username);

	if (!email.empty())
		set_config("user.email", email);
}

void git_tool::set_remote(
	std::string org, std::string key,
	bool no_push_upstream, bool push_default_origin)
{
	if (has_remote("upstream"))
	{
		cx().trace(context::generic, "upstream remote already exists");
		return;
	}

	const auto gf = git_file();

	rename_remote("origin", "upstream");

	if (no_push_upstream)
		set_remote_push("upstream", "nopushurl");

	add_remote("origin", org, key, push_default_origin, {}, gf);
}

void git_tool::rename_remote(const std::string& from, const std::string& to)
{
	run(details::rename_remote(root_, from, to));
}

void git_tool::set_config(const std::string& key, const std::string& value)
{
	run(details::set_config(root_, key, value));
}

void git_tool::set_remote_push(const std::string& remote, const std::string& url)
{
	run(details::set_remote_push(root_, remote, url));
}

void git_tool::set_assume_unchanged(const fs::path& file, bool on)
{
	run(details::set_assume_unchanged(root_, file, on));
}

void git_tool::ignore_ts(bool b)
{
	details::for_each_ts(root_, [&](auto&& p)
	{
		const auto rp = fs::relative(p, root_);

		if (is_tracked(rp))
		{
			cx().trace(context::generic, "  . {}", rp);
			set_assume_unchanged(rp, b);
		}
		else
		{
			cx().trace(context::generic, "  . {} (skipping, not tracked)", rp);
		}
	});
}

void git_tool::revert_ts()
{
	details::for_each_ts(root_, [&](auto&& p)
	{
		const auto rp = fs::relative(p, root_);

		if (!is_tracked(rp))
		{
			cx().debug(context::generic,
				"won't try to revert ts file '{}', not tracked", rp);

			return;
		}

		run(details::revert(root_, p));
	});
}

bool git_tool::is_tracked(const fs::path& file)
{
	return (run(details::is_tracked(root_, file)) == 0);
}

bool git_tool::has_remote(const std::string& name)
{
	return (run(details::has_remote(root_, name)) == 0);
}

void git_tool::add_remote(
	const std::string& remote_name,
	const std::string& username, const std::string& key, bool push_default,
	const std::string& url_pattern, const std::string& opt_git_file)
{
	auto gf = opt_git_file;
	if (gf.empty())
		gf = git_file();

	if (!has_remote(remote_name))
	{
		run(details::add_remote(
			root_, remote_name, make_url(username, gf, url_pattern)));

		if (push_default)
			set_config("remote.pushdefault", remote_name);

		if (!key.empty())
			set_config("remote." + remote_name + ".puttykeyfile", key);
	}
}

void git_tool::init_repo()
{
	run(details::init(root_));
}

void git_tool::apply(const std::string& diff)
{
	run(details::apply(root_, diff));
}

void git_tool::fetch(const std::string& remote, const std::string& branch)
{
	run(details::fetch(root_, remote, branch));
}

void git_tool::checkout(const std::string& what)
{
	run(details::checkout(root_, what));
}

std::string git_tool::current_branch()
{
	auto p = details::current_branch(root_);
	run(p);
	return trim_copy(p.stdout_string());
}

void git_tool::add_submodule(
	const std::string& branch, const std::string& submodule,
	const mob::url& url)
{
	run(details::add_submodule(root_, branch, submodule, url));
}

std::string git_tool::git_file()
{
	auto p = details::git_file(root_);
	run(p);
	const std::string out = p.stdout_string();

	const auto last_slash = out.find_last_of("/");
	if (last_slash == std::string::npos)
	{
		cx().error(context::generic, "bad get-url output '{}'", out);
		throw bailed();
	}

	auto s = trim_copy(out.substr(last_slash + 1));

	if (s.empty())
	{
		cx().error(context::generic, "bad get-url output '{}'", out);
		throw bailed();
	}

	return s;
}

void git_tool::delete_directory(const context& cx, const fs::path& dir)
{
	git_tool g(dir);

	if (!conf().global().get<bool>("ignore_uncommitted"))
	{
		if (g.has_uncommitted_changes())
		{
			cx.bail_out(context::redownload,
				"will not delete {}, has uncommitted changes; "
				"see --ignore-uncommitted-changes", dir);
		}

		if (g.has_stashed_changes())
		{
			cx.bail_out(context::redownload,
				"will not delete {}, has stashed changes; "
				"see --ignore-uncommitted-changes", dir);
		}
	}

	cx.trace(context::redownload,
		"deleting directory controlled by git {}", dir);

	op::delete_directory(cx, dir, op::optional);
}

bool git_tool::is_git_repo()
{
	return (run(details::is_repo(root_)) == 0);
}

bool git_tool::remote_branch_exists(const mob::url& u, const std::string& name)
{
	return (details::remote_branch_exists(u, name).run_and_join() == 0);
}

bool git_tool::has_uncommitted_changes()
{
	auto p = details::has_uncommitted_changes(root_);
	run(p);
	return (p.stdout_string() != "");
}

bool git_tool::has_stashed_changes()
{
	auto p = details::has_stashed_changes(root_);
	return (run(p) == 0);
}

std::string git_tool::make_url(
	const std::string& org, const std::string& git_file,
	const std::string& url_pattern)
{
	const std::string pattern = url_pattern.empty() ?
		details::default_github_url_pattern : url_pattern;

	return fmt::format(pattern, org, git_file);
}


git::git(ops o)
	: basic_process_runner("git"), op_(o)
{
}

git& git::url(const mob::url& u)
{
	url_ = u;
	return *this;
}

git& git::root(const fs::path& dir)
{
	root_ = dir;
	return *this;
}

git& git::branch(const std::string& name)
{
	branch_ = name;
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

git& git::credentials(
	const std::string& username, const std::string& email)
{
	creds_username_ = username;
	creds_email_ = email;
	return *this;
}

git& git::shallow(bool b)
{
	shallow_ = b;
	return *this;
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

		default:
		{
			cx().bail_out(context::generic, "git unknown op {}", op_);
		}
	}
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

	git_tool g(root_, this);

	g.clone(url_, branch_, shallow_);

	if (!creds_username_.empty() || !creds_email_.empty())
		g.set_credentials(creds_username_, creds_email_);

	if (!remote_org_.empty())
		g.set_remote(remote_org_, remote_key_, no_push_upstream_, push_default_origin_);

	if (ignore_ts_)
		g.ignore_ts(true);

	return true;
}

void git::do_pull()
{
	git_tool g(root_, this);

	if (revert_ts_)
		g.revert_ts();

	g.pull(url_, branch_);
}


git_submodule::git_submodule()
	: basic_process_runner("git submodule")
{
}

git_submodule& git_submodule::url(const mob::url& u)
{
	url_ = u;
	return *this;
}

git_submodule& git_submodule::root(const fs::path& dir)
{
	root_ = dir;
	return *this;
}

git_submodule& git_submodule::branch(const std::string& name)
{
	branch_ = name;
	return *this;
}

git_submodule& git_submodule::submodule(const std::string& name)
{
	submodule_ = name;
	return *this;
}

const std::string& git_submodule::submodule() const
{
	return submodule_;
}

void git_submodule::do_run()
{
	git_tool(root_, this).add_submodule(branch_, submodule_, url_);
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

void git_submodule_adder::queue(git_submodule g)
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
	std::vector<git_submodule> v;

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
				"git_submodule_adder: running {}", g.submodule());

			g.run(cx_);
		});

		if (quit_)
			break;
	}
}

}	// namespace

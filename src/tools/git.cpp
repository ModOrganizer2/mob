#include "pch.h"
#include "tools.h"
#include "../conf.h"

namespace mob
{

git::git(ops o)
	: basic_process_runner("git"), op_(o), ignore_ts_(false)
{
}

void git::set_credentials(
	const fs::path& repo,
	const std::string& username, const std::string& email)
{
	git(ops::none)
		.root(repo)
		.credentials(username, email)
		.do_set_credentials();
}

void git::set_remote(
	const fs::path& repo,
	std::string org, std::string key,
	bool no_push_upstream, bool push_default_origin)
{
	git(ops::none)
		.root(repo)
		.remote(org, key, no_push_upstream, push_default_origin)
		.do_set_remote();
}

void git::ignore_ts(const fs::path& repo, bool b)
{
	git(ops::none)
		.root(repo)
		.ignore_ts(b)
		.do_ignore_ts();
}

void git::add_remote(
	const fs::path& repo, const std::string& remote_name,
	const std::string& username, const std::string& key, bool push_default)
{
	git g(ops::none);
	g.root(repo);

	const auto gf = g.git_file();

	if (!g.has_remote(remote_name))
	{
		g.add_remote(remote_name, make_url(username, gf));

		if (push_default)
			g.set_config("remote.pushdefault", remote_name);

		if (!key.empty())
			g.set_config("remote." + remote_name + ".puttykeyfile", key);
	}
}

bool git::is_git_repo(const fs::path& p)
{
	git g(ops::none);
	g.root(p);
	return g.is_repo();
}

void git::init_repo(const fs::path& p)
{
	git g(ops::none);
	g.root(p);
	g.init();
}

fs::path git::binary()
{
	return conf::tool_by_name("git");
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

git& git::ignore_ts(bool b)
{
	ignore_ts_ = b;
	return *this;
}

void git::do_run()
{
	if (url_.empty() || root_.empty())
		bail_out("git missing parameters");

	if (conf::redownload() || conf::reextract())
	{
		cx().trace(context::rebuild, "deleting directory controlled by git");
		op::delete_directory(cx(), root_, op::optional);
	}


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

		case ops::none:
		default:
		{
			cx().bail_out(context::generic, "git unknown op {}", op_);
		}
	}
}

process git::make_process()
{
	static env e = this_env::get()
		.set("GCM_INTERACTIVE", "never")
		.set("GIT_TERMINAL_PROMPT", "0");

	return std::move(process()
		.binary(binary())
		.env(e));
}

void git::do_add_submodule()
{
	process_ = make_process()
		.stderr_level(context::level::trace)
		.arg("-c", "core.autocrlf=false")
		.arg("submodule")
		.arg("--quiet")
		.arg("add")
		.arg("-b", branch_)
		.arg("--force")
		.arg("--name", submodule_)
		.arg(url_)
		.arg(submodule_)
		.cwd(root_);

	execute_and_join();
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

	process_ = make_process()
		.stderr_level(context::level::trace)
		.arg("clone")
		.arg("--recurse-submodules")
		.arg("--depth", "1")
		.arg("--branch", branch_)
		.arg("--quiet", process::log_quiet)
		.arg("-c", "advice.detachedHead=false", process::log_quiet)
		.arg(url_)
		.arg(root_);

	execute_and_join();


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
	process_ = make_process()
		.stderr_level(context::level::trace)
		.arg("pull")
		.arg("--recurse-submodules")
		.arg("--quiet", process::log_quiet)
		.arg(url_)
		.arg(branch_)
		.cwd(root_);

	execute_and_join();
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

void git::do_ignore_ts()
{
	for (auto&& e : fs::recursive_directory_iterator(root_))
	{
		if (!e.is_regular_file())
			continue;

		const auto p = e.path();

		if (!path_to_utf8(p.extension()).ends_with(".ts"))
			continue;

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
	}
}

void git::set_config(const std::string& key, const std::string& value)
{
	process_ = make_process()
		.stderr_level(context::level::trace)
		.arg("config")
		.arg(key)
		.arg(value)
		.cwd(root_);

	execute_and_join();
}

bool git::has_remote(const std::string& name)
{
	process_ = make_process()
		.flags(process::allow_failure)
		.stderr_level(context::level::debug)
		.arg("remote")
		.arg("show")
		.arg(name)
		.cwd(root_);

	return (execute_and_join() == 0);
}

void git::rename_remote(const std::string& from, const std::string& to)
{
	process_ = make_process()
		.arg("remote")
		.arg("rename")
		.arg(from)
		.arg(to)
		.cwd(root_);

	execute_and_join();
}

void git::add_remote(const std::string& name, const std::string& url)
{
	process_ = make_process()
		.arg("remote")
		.arg("add")
		.arg(name)
		.arg(url)
		.cwd(root_);

	execute_and_join();
}

void git::set_remote_push(const std::string& remote, const std::string& url)
{
	process_ = make_process()
		.arg("remote")
		.arg("set-url")
		.arg("--push")
		.arg(remote)
		.arg(url)
		.cwd(root_);

	execute_and_join();
}

void git::set_assume_unchanged(const fs::path& relative_file, bool on)
{
	process_ = make_process()
		.arg("update-index")
		.arg(on ? "--assume-unchanged" : "--no-assume-unchanged")
		.arg(relative_file, process::forward_slashes)
		.cwd(root_);

	execute_and_join();
}

bool git::is_tracked(const fs::path& relative_file)
{
	process_ = make_process()
		.stdout_level(context::level::debug)
		.stderr_level(context::level::debug)
		.flags(process::allow_failure)
		.arg("ls-files")
		.arg("--error-unmatch")
		.arg(relative_file, process::forward_slashes)
		.cwd(root_);

	return (execute_and_join() == 0);
}

bool git::is_repo()
{
	process_ = make_process()
		.arg("rev-parse")
		.arg("--is-inside-work-tree")
		.stderr_filter([](process::filter& f)
		{
			if (f.line.find("not a git repo") != std::string::npos)
				f.lv = context::level::trace;
		})
		.flags(process::allow_failure)
		.cwd(root_);

	return (execute_and_join() == 0);
}

void git::init()
{
	process_ = make_process()
		.arg("init")
		.cwd(root_);

	execute_and_join();
}

std::string git::git_file()
{
	process_ = make_process()
		.stdout_flags(process::keep_in_string)
		.arg("remote")
		.arg("get-url")
		.arg("origin")
		.cwd(root_);

	execute_and_join();

	const std::string out = process_.stdout_string();

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
	const std::string& org, const std::string& git_file)
{
	return "git@github.com:" + org + "/" + git_file;
}

}	// namespace

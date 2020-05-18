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
	git(ops(0))
		.output(repo)
		.credentials(username, email)
		.do_set_credentials();
}

void git::set_remote(
	const fs::path& repo,
	std::string org, std::string key,
	bool no_push_upstream, bool push_default_origin)
{
	git(ops(0))
		.output(repo)
		.remote(org, key, no_push_upstream, push_default_origin)
		.do_set_remote();
}

void git::ignore_ts(const fs::path& repo, bool b)
{
	git(ops(0))
		.output(repo)
		.ignore_ts(b)
		.do_ignore_ts();
}

void git::add_remote(
	const fs::path& repo, const std::string& remote_name,
	const std::string& username, const std::string& key, bool push_default)
{
	git g(ops(0));
	g.output(repo);

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

git& git::output(const fs::path& dir)
{
	where_ = dir;
	return *this;
}

git& git::credentials(const std::string& username, const std::string& email)
{
	creds_username_ = username;
	creds_email_ = email;
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
	if (url_.empty() || where_.empty())
		bail_out("git missing parameters");

	if (conf::redownload() || conf::reextract())
	{
		cx().trace(context::rebuild, "deleting directory controlled by git");
		op::delete_directory(cx(), where_, op::optional);
	}


	switch (op_)
	{
		case clone:
		{
			do_clone();
			break;
		}

		case pull:
		{
			do_pull();
			break;
		}

		case clone_or_pull:
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
	const fs::path dot_git = where_ / ".git";
	if (fs::exists(dot_git))
	{
		cx().trace(context::generic, "not cloning, {} exists", dot_git);
		return false;
	}

	process_ = process()
		.binary(binary())
		.stderr_level(context::level::trace)
		.arg("clone")
		.arg("--recurse-submodules")
		.arg("--depth", "1")
		.arg("--branch", branch_)
		.arg("--quiet", process::log_quiet)
		.arg("-c", "advice.detachedHead=false", process::log_quiet)
		.arg(url_)
		.arg(where_);

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
	process_ = process()
		.binary(binary())
		.stderr_level(context::level::trace)
		.arg("pull")
		.arg("--recurse-submodules")
		.arg("--quiet", process::log_quiet)
		.arg(url_)
		.arg(branch_)
		.cwd(where_);

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
	for (auto&& e : fs::recursive_directory_iterator(where_))
	{
		if (!e.is_regular_file())
			continue;

		const auto p = e.path();

		if (!path_to_utf8(p.extension()).ends_with(".ts"))
			continue;

		const auto rp = fs::relative(p, where_);

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
	process_ = process()
		.binary(binary())
		.arg("config")
		.arg(key)
		.arg(value)
		.cwd(where_);

	execute_and_join();
}

bool git::has_remote(const std::string& name)
{
	process_ = process()
		.binary(binary())
		.flags(process::allow_failure)
		.stderr_level(context::level::debug)
		.arg("remote")
		.arg("show")
		.arg(name)
		.cwd(where_);

	return (execute_and_join() == 0);
}

void git::rename_remote(const std::string& from, const std::string& to)
{
	process_ = process()
		.binary(binary())
		.arg("remote")
		.arg("rename")
		.arg(from)
		.arg(to)
		.cwd(where_);

	execute_and_join();
}

void git::add_remote(const std::string& name, const std::string& url)
{
	process_ = process()
		.binary(git::binary())
		.arg("remote")
		.arg("add")
		.arg(name)
		.arg(url)
		.cwd(where_);

	execute_and_join();
}

void git::set_remote_push(const std::string& remote, const std::string& url)
{
	process_ = process()
		.binary(git::binary())
		.arg("remote")
		.arg("set-url")
		.arg("--push")
		.arg(remote)
		.arg(url)
		.cwd(where_);

	execute_and_join();
}

void git::set_assume_unchanged(const fs::path& relative_file, bool on)
{
	process_ = process()
		.binary(git::binary())
		.arg("update-index")
		.arg(on ? "--assume-unchanged" : "--no-assume-unchanged")
		.arg(relative_file, process::forward_slashes)
		.cwd(where_);

	execute_and_join();
}

bool git::is_tracked(const fs::path& relative_file)
{
	process_ = process()
		.binary(git::binary())
		.stdout_level(context::level::debug)
		.stderr_level(context::level::debug)
		.flags(process::allow_failure)
		.arg("ls-files")
		.arg("--error-unmatch")
		.arg(relative_file, process::forward_slashes)
		.cwd(where_);

	return (execute_and_join() == 0);
}

std::string git::git_file()
{
	process_ = process()
		.binary(git::binary())
		.stdout_flags(process::keep_in_string)
		.arg("remote")
		.arg("get-url")
		.arg("origin")
		.cwd(where_);

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

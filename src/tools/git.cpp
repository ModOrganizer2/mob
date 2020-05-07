#include "pch.h"
#include "tools.h"
#include "../conf.h"

namespace mob
{

url make_github_url(const std::string& org, const std::string& repo)
{
	return "https://github.com/" + org + "/" + repo + ".git";
}


git_clone::git_clone()
	: basic_process_runner("git")
{
}

git_clone& git_clone::url(const mob::url& u)
{
	url_ = u;
	return *this;
}

git_clone& git_clone::branch(const std::string& name)
{
	branch_ = name;
	return *this;
}

git_clone& git_clone::output(const fs::path& dir)
{
	where_ = dir;
	return *this;
}

void git_clone::do_run()
{
	if (url_.empty() || where_.empty())
		bail_out("git_clone missing parameters");

	if (conf::redownload() || conf::reextract())
	{
		cx_->trace(context::rebuild, "deleting directory controlled by git");
		op::delete_directory(*cx_, where_, op::optional);
	}

	const fs::path dot_git = where_ / ".git";

	if (!fs::exists(dot_git))
		clone();
	else
		pull();
}

void git_clone::clone()
{
	process_ = process()
		.binary(tools::git::binary())
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
}

void git_clone::pull()
{
	process_ = process()
		.binary(tools::git::binary())
		.stderr_level(context::level::trace)
		.arg("pull")
		.arg("--recurse-submodules")
		.arg("--quiet", process::log_quiet)
		.arg(url_)
		.arg(branch_)
		.cwd(where_);

	execute_and_join();
}

}	// namespace

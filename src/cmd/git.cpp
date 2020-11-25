#include "pch.h"
#include "commands.h"
#include "../tasks/tasks.h"
#include "../tools/tools.h"

namespace mob
{

git_command::git_command()
	: command(requires_options)
{
}

command::meta_t git_command::meta() const
{
	return
	{
		"git",
		"manages the git repos"
	};
}

clipp::group git_command::do_group()
{
	return clipp::group(
		clipp::command("git").set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% ("shows this message"),

		"set-remotes" %
		(clipp::command("set-remotes").set(mode_, modes::set_remotes),
			(clipp::required("-u", "--username")
				& clipp::value("USERNAME") >> username_)
				% "git username",

			(clipp::required("-e", "--email")
				& clipp::value("EMAIL") >> email_)
				% "git email",

			(clipp::option("-k", "--key")
				& clipp::value("PATH") >> key_)
				% "path to putty key",

			(clipp::option("-s", "--no-push").set(nopush_)
				% "disables pushing to 'upstream' by changing the push url "
				  "to 'nopushurl' to avoid accidental pushes"),

			(clipp::option("-p", "--push-origin").set(push_default_)
				% "sets the new 'origin' remote as the default push target"),

			(clipp::opt_value("path") >> path_)
				% "only use this repo"
		)

		|

		"add-remote" %
		(clipp::command("add-remote").set(mode_, modes::add_remote),
			(clipp::required("-n", "--name")
				& clipp::value("NAME") >> remote_)
				% "name of new remote",

			(clipp::required("-u", "--username")
				& clipp::value("USERNAME") >> username_)
				% "git username",

			(clipp::option("-k", "--key")
				& clipp::value("PATH") >> key_)
				% "path to putty key",

			(clipp::option("-p", "--push-origin").set(push_default_)
				% "sets this new remote as the default push target"),

			(clipp::opt_value("path") >> path_)
				% "only use this repo"
		)

		|

		"ignore-ts" %
		(clipp::command("ignore-ts").set(mode_, modes::ignore_ts),
			(
				clipp::command("on").set(tson_, true) |
				clipp::command("off").set(tson_, false)
			)
		)

		|

		"branches" %
		(clipp::command("branches").set(mode_, modes::branches),
			clipp::option("-a", "--all").set(all_branches_)
				% "shows all branches, including those on master"
		)
	);
}

int git_command::do_run()
{
	switch (mode_)
	{
		case modes::set_remotes:
		{
			do_set_remotes();
			break;
		}

		case modes::add_remote:
		{
			do_add_remote();
			break;
		}

		case modes::ignore_ts:
		{
			do_ignore_ts();
			break;
		}

		case modes::branches:
		{
			do_branches();
			break;
		}

		case modes::none:
		default:
			u8cerr << "bad git mode " << static_cast<int>(mode_) << "\n";
			throw bailed();
	}

	return 0;
}

std::string git_command::do_doc()
{
	return
		"All the commands will go through all modorganizer repos, plus usvfs\n"
		"and NCC.\n"
		"\n"
		"Commands:\n"
		"set-remotes\n"
		"  For each repo, this first sets the username and email. Then, it\n"
		"  will rename the remote 'origin' to 'upstream' and create a new\n"
		"  remote 'origin' with the given information. If the remote\n"
		"  'upstream' already exists in a repo, nothing happens.\n"
		"\n"
		"add-remote\n"
		"  For each repo, adds a new remote with the given information. If a\n"
		"  remote with the same name already exists, nothing happens.\n"
		"\n"
		"ignore-ts\n"
		"  Toggles the --assume-changed status of all .ts files in all repos.\n"
		"\n"
		"branches\n"
		"  Lists all git repos that are not on master. With -a, show all \n"
		"  repos and their current branch.";
}

void git_command::do_set_remotes()
{
	if (path_.empty())
	{
		const auto repos = get_repos();

		for (auto&& r : repos)
			do_set_remotes(r);
	}
	else
	{
		do_set_remotes(path_);
	}
}

void git_command::do_set_remotes(const fs::path& r)
{
	u8cout << "setting up " << path_to_utf8(r.filename()) << "\n";
	git_wrap(r).set_credentials(username_, email_);
	git_wrap(r).set_remote(username_, key_, nopush_, push_default_);
}

void git_command::do_add_remote()
{
	u8cout
		<< "adding remote '" << remote_ << "' "
		<< "from '" << username_ << "' to repos\n";

	if (path_.empty())
	{
		const auto repos = get_repos();

		for (auto&& r : repos)
			do_add_remote(r);
	}
	else
	{
		do_add_remote(path_);
	}
}

void git_command::do_add_remote(const fs::path& r)
{
	u8cout << path_to_utf8(r.filename()) << "\n";
	git_wrap(r).add_remote(remote_, username_, key_, push_default_);
}

void git_command::do_ignore_ts()
{
	if (tson_)
		u8cout << "ignoring .ts files\n";
	else
		u8cout << "un-ignoring .ts files\n";

	if (path_.empty())
	{
		const auto repos = get_repos();

		for (auto&& r : repos)
			do_ignore_ts(r);
	}
	else
	{
		do_ignore_ts(path_);
	}
}

void git_command::do_ignore_ts(const fs::path& r)
{
	u8cout << path_to_utf8(r.filename()) << "\n";
	git_wrap(r).ignore_ts(tson_);
}

void git_command::do_branches()
{
	std::vector<std::pair<std::string, std::string>> v;

	for (auto&& r : get_repos())
	{
		const auto b = git_wrap(r).current_branch();
		if (b == "master" && !all_branches_)
			continue;

		if (b.empty())
			v.push_back({r.filename().string(), "detached head"});
		else
			v.push_back({r.filename().string(), b});
	}

	u8cout << table(v, 0, 3) << "\n";
}

std::vector<fs::path> git_command::get_repos() const
{
	std::vector<fs::path> v;

	// usvfs
	if (fs::exists(tasks::usvfs::source_path()))
		v.push_back(tasks::usvfs::source_path());

	// ncc
	if (fs::exists(tasks::ncc::source_path()))
		v.push_back(tasks::ncc::source_path());


	const auto super = tasks::modorganizer::super_path();

	// all directories in super except for those starting with a dot
	if (fs::exists(super))
	{
		for (auto e : fs::directory_iterator(super))
		{
			if (!e.is_directory())
				continue;

			const auto p = e.path();
			if (path_to_utf8(p.filename()).starts_with("."))
				continue;

			v.push_back(p);
		}
	}

	return v;
}

}	// namespace

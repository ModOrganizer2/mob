#include "pch.h"
#include "commands.h"
#include "../tasks/tasks.h"

namespace mob
{

std::string read_file(const fs::path& p)
{
	std::ifstream t(p);
	return {std::istreambuf_iterator<char>(t), std::istreambuf_iterator<char>()};
}


pr_command::pr_command()
	: command(requires_options | handle_sigint), method_("apply")
{
}

command::meta_t pr_command::meta() const
{
	return
	{
		"pr",
		"applies changes from PRs"
	};
}

clipp::group pr_command::do_group()
{
	return
		(clipp::command("pr")).set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% ("shows this message"),

		(clipp::option("--method")
			& clipp::value("METHOD") >> method_)
			% "how to apply the changes, one of `apply` (default), `remote` or "
			  " `fetch`",

		(clipp::option("--github-token")
			& clipp::value("TOKEN") >> github_token_)
			% "github api key",

		(clipp::value("PR") >> pr_)
			% "PR to apply, must be `task/pr`, such as `modorganizer/123`";
}

std::string pr_command::do_doc()
{
	return
		"Methods:\n"
		"  - apply:  retrieves the .diff file for this PR and runs\n"
		"            `git apply` with it, which patches the local repo and\n"
		"            leaves the changes uncommitted\n"
		"\n"
		"  - remote: adds a new remote to the local repo that matches the \n"
		"            PR's origin and checks out the PR's branch from it\n"
		"\n"
		"  - fetch: ??";
}

int pr_command::do_run()
{
	if (method_ == "apply")
		return do_apply();
	else if (method_ == "remote")
		return do_remote();
	else if (method_ == "fetch")
		return do_fetch();

	u8cerr << "bad method '" << method_ << "'\n";
	return 1;
}

std::pair<const modorganizer*, std::string> pr_command::parse_pr(
	const std::string& pr) const
{
	if (pr.empty())
		return {};

	const auto cs = split(pr, "/");
	if (cs.size() != 2)
	{
		u8cerr << "--pr must be task/pr, such as modorganizer/123\n";
		return {};
	}

	const std::string pattern = cs[0];
	const std::string pr_number = cs[1];

	const auto tasks = find_tasks(pattern);

	if (tasks.empty())
	{
		u8cerr << "no task matches '" << pattern << "'\n";
		return {};
	}
	else if (tasks.size() > 1)
	{
		u8cerr
			<< "found " << tasks.size() << " matches for pattern "
			<< "'" << pattern << "'\n"
			<< "the pattern must only match one task\n";

		return {};
	}

	const auto* task = dynamic_cast<modorganizer*>(tasks[0]);
	if (!task)
	{
		u8cerr << "only modorganizer tasks are supported\n";
		return {};
	}

	return {task, pr_number};
}

int pr_command::do_apply()
{
	auto&& [task, pr] = parse_pr(pr_);
	if (!task)
		return 1;

	const auto u = get_diff_url(task, pr);

	curl_downloader dl;

	dl
		.url(u)
		.start()
		.join();

	if (!dl.ok())
	{
		u8cerr << "getting pr diff failed\n";
		return 1;
	}

	const auto diff = dl.steal_output();
	git::apply(task->this_source_path(), diff);

	return 0;
}

int pr_command::do_remote()
{
	auto&& [task, pr] = parse_pr(pr_);
	if (!task)
		return 1;

	try
	{
		u8cout << "looking for pr " << pr << " in " << task->name() << "\n";

		//const auto json = get_pr_info(task, pr);
		auto json = nlohmann::json::parse(read_file("c:\\tmp\\" + pr + ".json"));
		if (json.empty())
			return 1;

		const std::string repo = json["head"]["repo"]["name"];
		const std::string org = json["head"]["repo"]["owner"]["login"];
		const std::string remote_name = org;
		const std::string branch = json["head"]["ref"];

		u8cout
			<< "found pr: "
			<< "repo=" << repo << " "
			<< "org=" << org << " "
			<< "branch=" << branch << "\n";

		if (git::has_remote(task->this_source_path(), remote_name))
		{
			u8cout << "remote already exists\n";
		}
		else
		{
			u8cout << "adding remote " << remote_name << "\n";

			git::add_remote(
				task->this_source_path(),
				remote_name, org, {}, false,
				"https://github.com/{}/{}");
		}

		u8cout << "fetching from " << remote_name << "\n";
		git::fetch(task->this_source_path(), remote_name, branch);

		u8cout << "checking out " << remote_name << "/" << branch << "\n";
		git::checkout(
			task->this_source_path(),
			::fmt::format("{}/{}", remote_name, branch));

		return 0;
	}
	catch(std::exception& e)
	{
		u8cerr << e.what() << "\n";
		return 1;
	}
}

int pr_command::do_fetch()
{
	return 0;
}

url pr_command::get_diff_url(const modorganizer* task, const std::string& pr)
{
	return ::fmt::format(
		"https://github.com/{}/{}/pull/{}.diff",
		task->org(), task->repo(), pr);
}

nlohmann::json pr_command::get_pr_info(
	const modorganizer* task, const std::string& pr)
{
	if (github_token_.empty())
	{
		u8cerr << "missing --github-token\n";
		return {};
	}

	const url u(::fmt::format(
		"https://api.github.com/repos/{}/{}/pulls/{}",
		task->org(), task->repo(), pr));

	curl_downloader dl;

	dl
		.url(u)
		.header("Authorization", "token " + github_token_)
		.start()
		.join();

	if (!dl.ok())
	{
		u8cerr << "failed to get pr info from github\n";
		return {};
	}

	const auto output = dl.steal_output();
	return nlohmann::json::parse(output);
}

}	// namespace

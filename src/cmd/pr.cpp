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

std::string pr_command::do_doc()
{
	return {};
}

clipp::group pr_command::do_group()
{
	return
		(clipp::command("pr")).set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% ("shows this message"),

		(clipp::option("--github-token")
			& clipp::value("TOKEN") >> github_token_)
			% "github api key",

		(clipp::value("PR") >> pr_)
			% "PR to apply, must be `task/pr`, such as `modorganizer/123`";
}

int pr_command::do_run()
{
	if (const auto r=pull_pr(); r != 0)
		return r;

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

int pr_command::pull_pr()
{
	auto&& [task, pr] = parse_pr(pr_);
	if (!task)
		return 1;

	try
	{
		u8cout << "fetching pr " << pr << " in " << task->name() << "\n";
		git::fetch(
			task->this_source_path(),
			task->git_url().string(), ::fmt::format("pull/{}/head", pr));

		u8cout << "checking out FETCH_HEAD\n";
		git::checkout(task->this_source_path(), "FETCH_HEAD");

		u8cout << "note: " << task->name() << " is in detached HEAD state\n";

		return 0;
	}
	catch(std::exception& e)
	{
		u8cerr << e.what() << "\n";
		return 1;
	}
}

pr_command::pr_info pr_command::get_pr_info(
	const modorganizer* task, const std::string& pr)
{
	constexpr bool from_file = true;

	nlohmann::json json;

	if constexpr (from_file)
	{
		json = nlohmann::json::parse(read_file("c:\\tmp\\" + pr + ".json"));
		if (json.empty())
			return {};
	}
	else
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
		json = nlohmann::json::parse(output);
	}

	const std::string repo = json["head"]["repo"]["name"];
	const std::string org = json["head"]["repo"]["owner"]["login"];
	const std::string branch = json["head"]["ref"];

	return {repo, org, branch};
}

}	// namespace

#include "pch.h"
#include "commands.h"
#include "../tasks/tasks.h"

namespace mob
{

pr_command::pr_command()
	: command(requires_options | handle_sigint)
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
		(clipp::command("build")).set(picked_),

		(clipp::option("-h", "--help") >> help_)
		% ("shows this message"),

		(clipp::option("--pr")
			& clipp::value("PR") >> pr_)
		% "checks out the branch of the given PR, must be `task/pr`, such as "
		"`modorganizer/123`",

		(clipp::option("--github-token")
			& clipp::value("TOKEN") >> github_token_)
		% "github api key for --pr";
}

int pr_command::do_run()
{
	//if (auto r=get_pr_branch(); r != 0)
	//	return r;

	return apply_pr_diff();
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

int pr_command::get_pr_branch()
{
	if (pr_.empty())
		return 0;

	if (github_token_.empty())
	{
		u8cerr << "missing --github-token\n";
		return 1;
	}

	auto&& [task, pr] = parse_pr(pr_);

	if (!task)
		return 1;

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
		u8cerr << "getting pr failed\n";
		return 1;
	}

	const auto output = dl.steal_output();
	u8cout << output << "\n";


	nlohmann::json j(output);

	const std::string diff_url = j["diff_url"];

	return 0;
}

int pr_command::apply_pr_diff()
{
	if (pr_.empty())
		return 0;

	auto&& [task, pr] = parse_pr(pr_);

	if (!task)
		return 1;

	const url u = ::fmt::format(
		"https://github.com/{}/{}/pull/{}.diff",
		task->org(), task->repo(), pr);


	//curl_downloader dl;
	//
	//dl
	//	.url(u)
	//	.header("Authorization", "token " + github_token_)
	//	.start()
	//	.join();
	//
	//if (!dl.ok())
	//{
	//	u8cerr << "getting pr diff failed\n";
	//	return 1;
	//}
	//
	//const auto output = dl.steal_output();
	//u8cout << output << "\n";

	std::ifstream t("c:\\tmp\\1277.diff");
	std::string output((std::istreambuf_iterator<char>(t)),
		std::istreambuf_iterator<char>());

	git::apply(task->this_source_path(), output);

	return 0;
}

}	// namespace

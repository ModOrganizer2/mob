#include "pch.h"
#include "commands.h"
#include "../tasks/tasks.h"

namespace mob
{

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

url pr_command::get_diff_url(const modorganizer* task, std::string pr)
{
	return ::fmt::format(
		"https://github.com/{}/{}/pull/{}.diff",
		task->org(), task->repo(), pr);
}

int pr_command::do_apply()
{
	if (pr_.empty())
		return 0;

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
	return 0;
}

int pr_command::do_fetch()
{
	return 0;
}

}	// namespace

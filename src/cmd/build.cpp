#include "pch.h"
#include "commands.h"
#include "../tasks/tasks.h"

namespace mob
{

constexpr bool do_timings = false;

build_command::build_command()
	: command(requires_options | handle_sigint)
{
}

command::meta_t build_command::meta() const
{
	return
	{
		"build",
		"builds tasks"
	};
}

clipp::group build_command::do_group()
{
	return
		(clipp::command("build")).set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% ("shows this message"),

		(clipp::option("-g", "--redownload") >> redownload_)
			% "redownloads archives, see --reextract",

		(clipp::option("-e", "--reextract") >> reextract_)
			% "deletes source directories and re-extracts archives",

		(clipp::option("-c", "--reconfigure") >> reconfigure_)
			% "reconfigures the task by running cmake, configure scripts, "
			   "etc.; some tasks might have to delete the whole source "
			   "directory",

		(clipp::option("-b", "--rebuild") >> rebuild_)
			%  "cleans and rebuilds projects; some tasks might have to "
			   "delete the whole source directory",

		(clipp::option("-n", "--new") >> new_)
			% "deletes everything and starts from scratch",

		(
			clipp::option("--clean-task").call([&]{ clean_ = true; }) |
			clipp::option("--no-clean-task").call([&]{ clean_ = false; })
		) % "sets whether tasks are cleaned",

		(
			clipp::option("--fetch-task").call([&]{ fetch_ = true; }) |
			clipp::option("--no-fetch-task").call([&]{ fetch_ = false; })
		) % "sets whether tasks are fetched",

		(
			clipp::option("--build-task").call([&]{ build_ = true; }) |
			clipp::option("--no-build-task").call([&]{ build_ = false; })
		) % "sets whether tasks are built",

		(
			clipp::option("--pull").call([&]{ nopull_ = false; }) |
			clipp::option("--no-pull").call([&]{ nopull_ = true; })
		) % "whether to pull repos that are already cloned; global override",

		(clipp::option("--pr")
			& clipp::value("PR") >> pr_)
			% "checks out the branch of the given PR, must be `task/pr`, such as "
			  "`modorganizer/123`",

		(clipp::option("--github-token")
			& clipp::value("TOKEN") >> github_token_)
			% "github api key for --pr",

		(
			clipp::option("--revert-ts").call([&]{ revert_ts_ = true; }) |
			clipp::option("--no-revert-ts").call([&]{ revert_ts_ = false; })
		) % "whether to revert all the .ts files in a repo before pulling to "
		    "avoid merge errors; global override",

		(clipp::option("--ignore-uncommitted-changes") >> ignore_uncommitted_)
			% "when --reextract is given, directories controlled by git will "
			  "be deleted even if they contain uncommitted changes",

		(clipp::option("--keep-msbuild") >> keep_msbuild_)
			% "don't terminate msbuild.exe instances after building",

		(clipp::opt_values(
			clipp::match::prefix_not("-"), "task", tasks_))
			% "tasks to run; specify 'super' to only build modorganizer "
			"projects";
}

void build_command::convert_cl_to_conf()
{
	command::convert_cl_to_conf();

	if (redownload_ || new_)
		common.options.push_back("global/redownload=true");

	if (reextract_ || new_)
		common.options.push_back("global/reextract=true");

	if (reconfigure_ || new_)
		common.options.push_back("global/reconfigure=true");

	if (rebuild_ || new_)
		common.options.push_back("global/rebuild=true");

	if (ignore_uncommitted_)
		common.options.push_back("global/ignore_uncommitted=true");

	if (clean_)
	{
		if (*clean_)
			common.options.push_back("global/clean_task=true");
		else
			common.options.push_back("global/clean_task=false");
	}

	if (fetch_)
	{
		if (*fetch_)
			common.options.push_back("global/fetch_task=true");
		else
			common.options.push_back("global/fetch_task=false");
	}

	if (build_)
	{
		if (*build_)
			common.options.push_back("global/build_task=true");
		else
			common.options.push_back("global/build_task=false");
	}

	if (nopull_)
	{
		if (*nopull_)
			common.options.push_back("_override:task/no_pull=true");
		else
			common.options.push_back("_override:task/no_pull=false");
	}

	if (revert_ts_)
	{
		if (*revert_ts_)
			common.options.push_back("_override:task/revert_ts=true");
		else
			common.options.push_back("_override:task/revert_ts=false");
	}

	if (!tasks_.empty())
		set_task_enabled_flags(tasks_);
}

int build_command::do_run()
{
	try
	{
		create_prefix_ini();

		//if (auto r=get_pr_branch(); r != 0)
		//	return r;

		apply_pr_diff();


		run_all_tasks();

		if (do_timings)
			dump_timings();

		if (!keep_msbuild_)
			terminate_msbuild();

		mob::gcx().info(mob::context::generic, "mob done");
		return 0;
	}
	catch(bailed&)
	{
		error("bailing out");
		return 1;
	}
}

void build_command::create_prefix_ini()
{
	// creating prefix
	if (!exists(paths::prefix()))
		op::create_directories(gcx(), paths::prefix());

	const auto ini = paths::prefix() / default_ini_filename();
	if (!exists(ini))
	{
		std::ofstream(ini)
			<< "[paths]\n"
			<< "prefix = .\n";
	}
}

std::pair<const modorganizer*, std::string> build_command::parse_pr(
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

int build_command::get_pr_branch()
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

int build_command::apply_pr_diff()
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


void build_command::dump_timings()
{
	using namespace std::chrono;

	std::ofstream out("timings.txt");

	// generates a file with line being "task,start_time,end_time,step"
	//
	// uibase,0,1,fetch
	// uibase,1,2,configure
	// uibase,2,3,build
	// modorganizer,4,5,fetch
	// modorganizer,5,6,configure
	// modorganizer,6,7,build

	auto write = [&](auto&& inst)
	{
		for (auto&& t : inst.instrumented_tasks())
		{
			for (auto&& tp : t.tps)
			{
				const auto start_ms = static_cast<double>(
					duration_cast<milliseconds>(tp.start).count());

				const auto end_ms = static_cast<double>(
					duration_cast<milliseconds>(tp.end).count());

				out
					<< inst.instrumentable_name() << "\t"
					<< (start_ms / 1000.0) << "\t"
					<< (end_ms / 1000.0) << "\t"
					<< t.name << "\n";
			}
		}
	};

	for (auto&& tk : get_all_tasks())
		write(*tk);

	write(git_submodule_adder::instance());
}

void build_command::terminate_msbuild()
{
	if (conf::dry())
		return;

	system("taskkill /im msbuild.exe /f > NUL 2>&1");
}

}	// namespace

#include "pch.h"
#include "tasks.h"

namespace mob
{

ncc::ncc()
	: basic_task("ncc")
{
}

std::string ncc::version()
{
	return {};
}

bool ncc::prebuilt()
{
	return false;
}

fs::path ncc::source_path()
{
	return paths::build() / "NexusClientCli";
}

void ncc::do_clean_for_rebuild()
{
	instrument<times::clean>([&]
	{
		op::delete_directory(
			cx(), source_path() / "NexusClientCLI" / "obj", op::optional);
	});
}

void ncc::do_fetch()
{
	instrument<times::fetch>([&]
	{
		run_tool(task_conf().make_git()
			.url(task_conf().make_git_url(task_conf().mo_org(), "modorganizer-NCC"))
			.branch(task_conf().mo_branch())
			.root(source_path()));
	});
}

void ncc::do_build_and_install()
{
	instrument<times::build>([&]
	{
		run_tool(msbuild()
			.solution(source_path() / "NexusClient.sln")
			.projects({"NexusClientCLI"})
			.platform("Any CPU"));
	});

	instrument<times::install>([&]
	{
		const auto publish = source_path() / "publish.bat";

		run_tool(process_runner(process()
			.binary(publish)
			.stderr_level(context::level::trace)
			.arg(paths::install_bin())));
	});
}

}	// namespace

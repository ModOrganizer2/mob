#include "pch.h"
#include "tasks.h"

namespace mob
{

ncc::ncc()
	: basic_task("ncc")
{
}

const std::string& ncc::version()
{
	static std::string s;
	return s;
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
	op::delete_directory(
		cx(), source_path() / "NexusClientCLI" / "obj", op::optional);
}

void ncc::do_fetch()
{
	run_tool(git(git::clone_or_pull)
		.url(make_github_url(conf::mo_org(), "modorganizer-NCC"))
		.branch(conf::mo_branch())
		.output(source_path()));
}

void ncc::do_build_and_install()
{
	run_tool(msbuild()
		.solution(source_path() / "NexusClient.sln")
		.projects({"NexusClientCLI"})
		.platform("Any CPU"));

	const auto publish =source_path() / "publish.bat";

	run_tool(process_runner(process()
		.binary(publish)
		.stderr_level(context::level::trace)
		.arg(paths::install_bin())));
}

}	// namespace

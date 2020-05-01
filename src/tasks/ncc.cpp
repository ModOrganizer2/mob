#include "pch.h"
#include "tasks.h"

namespace builder
{

ncc::ncc()
	: basic_task("ncc")
{
}

fs::path ncc::source_path()
{
	return paths::build() / "NexusClientCli";
}

void ncc::do_fetch()
{
	run_tool(git_clone()
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

	run_tool(process_runner(arch::dont_care, publish, cmd::noflags)
		.arg(paths::install_bin()));
}

}	// namespace

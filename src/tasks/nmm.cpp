#include "pch.h"
#include "tasks.h"

namespace builder
{

nmm::nmm()
	: basic_task("nmm")
{
}

fs::path nmm::source_path()
{
	return paths::build() / "Nexus-Mod-Manager";
}

void nmm::do_fetch()
{
	run_tool(git_clone()
		.url(make_github_url("Nexus-Mods", "Nexus-Mod-Manager"))
		.branch(versions::nmm())
		.output(source_path()));

	run_tool(nuget(source_path() / "NexusClient.sln"));
}

void nmm::do_build_and_install()
{
	run_tool(msbuild()
		.solution(source_path() / "NexusClient.sln")
		.platform("Any CPU"));
}

}	// namespace

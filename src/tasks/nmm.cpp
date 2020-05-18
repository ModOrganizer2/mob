#include "pch.h"
#include "tasks.h"

namespace mob
{

nmm::nmm()
	: basic_task("nmm")
{
}

std::string nmm::version()
{
	return conf::version_by_name("nmm");
}

bool nmm::prebuilt()
{
	return false;
}

fs::path nmm::source_path()
{
	return paths::build() / "Nexus-Mod-Manager";
}

void nmm::do_clean_for_rebuild()
{
	op::delete_directory(cx(), source_path() / "Stage", op::optional);
}

void nmm::do_fetch()
{
	run_tool(task_conf().make_git()
		.url(make_github_url("Nexus-Mods", "Nexus-Mod-Manager"))
		.branch(version())
		.output(source_path()));

	run_tool(nuget(source_path() / "NexusClient.sln"));
}

void nmm::do_build_and_install()
{
	// nmm sometimes fails with files being locked
	const int max_tries = 3;

	for (int tries=0; tries<max_tries; ++tries)
	{
		const int exit_code = run_tool(msbuild()
			.solution(source_path() / "NexusClient.sln")
			.platform("Any CPU")
			.flags(msbuild::allow_failure));

		if (exit_code == 0)
			return;

		cx().debug(context::generic,
			"msbuild multiprocess sometimes fails with nmm because of race "
			"conditions; trying again");
	}

	cx().debug(context::generic,
		"msbuild multiprocess has failed more than {} times for nmm, "
		"restarting one last time single process; that one should work",
		max_tries);

	run_tool(msbuild()
		.solution(source_path() / "NexusClient.sln")
		.platform("Any CPU")
		.flags(msbuild::single_job));
}

}	// namespace

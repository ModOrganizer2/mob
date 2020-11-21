#include "pch.h"
#include "tasks.h"

namespace mob::tasks
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
	return conf().paths().build() / "Nexus-Mod-Manager";
}

void nmm::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (is_set(c, clean::reclone))
		{
			git::delete_directory(cx(), source_path());
			return;
		}

		if (is_set(c, clean::rebuild))
			run_tool(create_msbuild_tool(msbuild::clean));
	});
}

void nmm::do_fetch()
{
	instrument<times::fetch>([&]
	{
		run_tool(task_conf().make_git()
			.url(task_conf().make_git_url("Nexus-Mods", "Nexus-Mod-Manager"))
			.branch(version())
			.root(source_path()));

		run_tool(nuget(source_path() / "NexusClient.sln"));
	});

}

void nmm::do_build_and_install()
{
	instrument<times::build>([&]
	{
		// nmm sometimes fails with files being locked
		const int max_tries = 3;

		for (int tries=0; tries<max_tries; ++tries)
		{
			const int exit_code = run_tool(create_msbuild_tool(
				msbuild::build, msbuild::allow_failure));

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

		run_tool(create_msbuild_tool(msbuild::build, msbuild::single_job));
	});
}

msbuild nmm::create_msbuild_tool(msbuild::ops o, msbuild::flags_t f)
{
	return std::move(msbuild(o)
		.solution(source_path() / "NexusClient.sln")
		.platform("Any CPU")
		.flags(f));
}

}	// namespace

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
	return conf().version().get("nmm");
}

bool nmm::prebuilt()
{
	return false;
}

fs::path nmm::source_path()
{
	return conf().path().build() / "Nexus-Mod-Manager";
}

void nmm::do_clean(clean c)
{
	// delete the whole directory
	if (is_set(c, clean::reclone))
	{
		git_wrap::delete_directory(cx(), source_path());

		// no need to do anything else
		return;
	}

	// msbuild clean
	if (is_set(c, clean::rebuild))
		run_tool(create_msbuild_tool(msbuild::clean));
}

void nmm::do_fetch()
{
	// clone/pull
	run_tool(make_git()
		.url(make_git_url("Nexus-Mods", "Nexus-Mod-Manager"))
		.branch(version())
		.root(source_path()));

	// run nuget
	run_tool(nuget(source_path() / "NexusClient.sln"));
}

void nmm::do_build_and_install()
{
	build_loop(cx(), [&](bool mp)
	{
		// msbuild defaults to multiprocess, give allow_failure for multiprocess
		// builds and force single_job for the last single process build

		const int exit_code = run_tool(create_msbuild_tool(
			msbuild::build,
			mp ? msbuild::allow_failure : msbuild::single_job));

		return (exit_code == 0);
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

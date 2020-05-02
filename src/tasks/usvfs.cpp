#include "pch.h"
#include "tasks.h"

namespace mob
{

usvfs::usvfs()
	: basic_task("usvfs")
{
}

fs::path usvfs::source_path()
{
	return paths::build() / "usvfs";
}

void usvfs::do_fetch()
{
	run_tool(git_clone()
		.url(make_github_url(conf::mo_org(), "usvfs"))
		.branch(versions::usvfs())
		.output(source_path()));
}

void usvfs::do_build_and_install()
{
	// usvfs doesn't use "Win32" for 32-bit, it uses "x86"
	//
	// note that usvfs_proxy has a custom build step in Release that runs
	// usvfs/vsbuild/stage_helper.cmd, which copies everything into install/

	run_tool(msbuild()
		.platform("x64")
		.solution(source_path() / "vsbuild" / "usvfs.sln"));

	run_tool(msbuild()
		.platform("x86")
		.solution(source_path() / "vsbuild" / "usvfs.sln"));
}

}	// namespace

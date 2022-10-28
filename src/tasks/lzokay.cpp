#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

lzokay::lzokay()
	: basic_task("lzokay")
{}

std::string lzokay::version()
{
	return conf().version().get("lzokay");
}

bool lzokay::prebuilt()
{
	return false;
}

fs::path lzokay::source_path()
{
	return conf().path().build() / ("lzokay-" + version());
}

void lzokay::do_clean(clean c)
{
	// delete the whole directory
	if (is_set(c, clean::reclone))
		git_wrap::delete_directory(cx(), source_path());
}

void lzokay::do_fetch()
{
	run_tool(make_git()
		.url(make_git_url("jackoalan", "lzokay"))
		.branch(version())
		.root(source_path()));
}

void lzokay::do_build_and_install()
{
	const fs::path build_path = run_tool(create_cmake_tool());

	run_tool(create_msbuild_tool());
}

cmake lzokay::create_cmake_tool(cmake::ops o)
{
	return std::move(cmake(o)
		.generator(cmake::vs)
		.root(source_path())
		.prefix(source_path()));
}

msbuild lzokay::create_msbuild_tool(msbuild::ops o)
{
	const fs::path build_path = create_cmake_tool().build_path();
	return std::move(msbuild(o).solution(build_path / "INSTALL.vcxproj"));
}

}	// namespace

#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

zlib::zlib()
	: basic_task("zlib")
{
}

std::string zlib::version()
{
	return conf().version().get("zlib");
}

bool zlib::prebuilt()
{
	return false;
}

fs::path zlib::source_path()
{
	return conf().path().build() / ("zlib-" + version());
}

void zlib::do_clean(clean c)
{
	// delete the whole directory
	if (is_set(c, clean::reclone))
	{
		git_wrap::delete_directory(cx(), source_path());

		// no point in doing anything more
		return;
	}

	// cmake clean
	if (is_set(c, clean::reconfigure))
		run_tool(create_cmake_tool(cmake::clean));

	// msbuild clean
	if (is_set(c, clean::rebuild))
		run_tool(create_msbuild_tool(msbuild::clean));
}

void zlib::do_fetch()
{
	run_tool(make_git()
		.url(make_git_url("madler", "zlib"))
		.branch(version())
		.root(source_path()));
}

void zlib::do_build_and_install()
{
	const fs::path build_path = run_tool(create_cmake_tool());

	run_tool(create_msbuild_tool());

	// zconf.h needs to be copied to the root directory, it's where python
	// looks for it
	op::copy_file_to_dir_if_better(cx(),
		build_path / "zconf.h",
		source_path());
}

cmake zlib::create_cmake_tool(cmake::ops o)
{
	return std::move(cmake(o)
		.generator(cmake::vs)
		.root(source_path())
		.arg("-Wno-deprecated")
		.prefix(source_path()));
}

msbuild zlib::create_msbuild_tool(msbuild::ops o)
{
	const fs::path build_path = create_cmake_tool().build_path();
	return std::move(msbuild(o).solution(build_path / "INSTALL.vcxproj"));
}

}	// namespace

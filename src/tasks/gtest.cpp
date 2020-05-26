#include "pch.h"
#include "tasks.h"

namespace mob
{

gtest::gtest()
	: basic_task("gtest", "googletest")
{
}

std::string gtest::version()
{
	return conf::version_by_name("gtest");
}

bool gtest::prebuilt()
{
	return false;
}

fs::path gtest::source_path()
{
	return paths::build() / "googletest";
}

void gtest::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (is_set(c, clean::reclone))
		{
			git::delete_directory(cx(), source_path());
			return;
		}

		if (is_set(c, clean::reconfigure))
		{
			run_tool(create_cmake_tool(arch::x86, cmake::clean));
			run_tool(create_cmake_tool(arch::x64, cmake::clean));
		}

		if (is_set(c, clean::rebuild))
		{
			run_tool(create_msbuild_tool(arch::x86, msbuild::clean));
			run_tool(create_msbuild_tool(arch::x64, msbuild::clean));
		}
	});
}

void gtest::do_fetch()
{
	instrument<times::fetch>([&]
	{
		run_tool(task_conf().make_git()
			.url(task_conf().make_git_url("google", "googletest"))
			.branch(version())
			.root(source_path()));
	});
}

cmake gtest::create_cmake_tool(arch a, cmake::ops o)
{
	const std::string build_dir = (a == arch::x64 ? "build" : "build_32");

	return std::move(cmake(o)
		.generator(cmake::vs)
		.architecture(a)
		.prefix(source_path() / build_dir)
		.root(source_path()));
}

msbuild gtest::create_msbuild_tool(arch a, msbuild::ops o)
{
	const fs::path build_path = create_cmake_tool(a).build_path();

	return std::move(msbuild(o)
		.architecture(a)
		.solution(build_path / "INSTALL.vcxproj"));
}

void gtest::do_build_and_install()
{
	instrument<times::build>([&]{
		parallel({
			{"gtest64", [&] {
				run_tool(create_cmake_tool(arch::x64));
				run_tool(create_msbuild_tool(arch::x64));
			}},

			{"gtest32", [&] {
				run_tool(create_cmake_tool(arch::x86));
				run_tool(create_msbuild_tool(arch::x86));
			}}
		});
	});
}

}	// namespace

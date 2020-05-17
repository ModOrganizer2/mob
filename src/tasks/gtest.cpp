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
	return version_by_name("gtest");
}

bool gtest::prebuilt()
{
	return false;
}

fs::path gtest::source_path()
{
	return paths::build() / "googletest";
}

void gtest::do_clean_for_rebuild()
{
	cmake::clean(cx(), source_path());
}

void gtest::do_fetch()
{
	run_tool(git(git::clone_or_pull)
		.url(make_github_url("google", "googletest"))
		.branch(version())
		.output(source_path()));
}

void gtest::do_build_and_install()
{
	parallel({
		{"gtest64", [&] {
			// x64
			const auto build_path_x64 = run_tool(cmake()
				.generator(cmake::vs)
				.architecture(arch::x64)
				.prefix(source_path() / "build")
				.root(source_path()));

			run_tool(msbuild()
				.solution(build_path_x64 / "INSTALL.vcxproj"));
		}},

		{"gtest32", [&] {
			// x86
			const auto build_path_x86 = run_tool(cmake()
				.generator(cmake::vs)
				.architecture(arch::x86)
				.prefix(source_path() / "build_32")
				.root(source_path()));

			run_tool(msbuild()
				.architecture(arch::x86)
				.solution(build_path_x86 / "INSTALL.vcxproj"));
		}}
	});
}

}	// namespace

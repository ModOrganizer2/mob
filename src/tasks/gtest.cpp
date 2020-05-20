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

void gtest::do_clean_for_rebuild()
{
	instrument(times_.clean, [&]
	{
		cmake::clean(cx(), source_path());
	});
}

void gtest::do_fetch()
{
	instrument(times_.fetch, [&]
	{
		run_tool(task_conf().make_git()
			.url(make_github_url("google", "googletest"))
			.branch(version())
			.root(source_path()));
	});
}

void gtest::do_build_and_install()
{
	instrument(times_.build, [&]{ parallel({
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
	});});
}

}	// namespace

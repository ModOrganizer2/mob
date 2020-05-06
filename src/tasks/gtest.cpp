#include "pch.h"
#include "tasks.h"

namespace mob
{

gtest::gtest()
	: basic_task("gtest", "googletest")
{
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
	run_tool(git_clone()
		.url(make_github_url("google", "googletest"))
		.branch(versions::gtest())
		.output(source_path()));
}

void gtest::do_build_and_install()
{
	parallel({
		{"gtest64", [&] {
			// x64
			const auto build_path_x64 = run_tool(cmake()
				.generator(cmake::jom)
				.architecture(arch::x64)
				.root(source_path()));

			run_tool(jom()
				.architecture(arch::x64)
				.path(build_path_x64));
		}},

		{"gtest32", [&] {
			// x86
			const auto build_path_x86 = run_tool(cmake()
				.generator(cmake::jom)
				.architecture(arch::x86)
				.root(source_path()));

			run_tool(jom()
				.architecture(arch::x86)
				.path(build_path_x86));
		}}
	});
}

}	// namespace

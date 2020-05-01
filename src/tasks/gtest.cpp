#include "pch.h"
#include "tasks.h"

namespace builder
{

gtest::gtest()
	: basic_task("gtest")
{
}

fs::path gtest::source_path()
{
	return paths::build() / "googletest";
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
	// x64
	const auto build_path_x64 = run_tool(cmake()
		.generator(cmake::jom)
		.architecture(arch::x64)
		.root(source_path()));

	run_tool(jom()
		.architecture(arch::x64)
		.path(build_path_x64));


	// x86
	const auto build_path_x86 = run_tool(cmake()
		.generator(cmake::jom)
		.architecture(arch::x86)
		.root(source_path()));

	run_tool(jom()
		.architecture(arch::x86)
		.path(build_path_x86));
}

}	// namespace

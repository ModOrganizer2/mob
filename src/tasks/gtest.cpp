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
		.org("google")
		.repo("googletest")
		.branch(versions::gtest())
		.output(source_path()));
}

void gtest::do_build_and_install()
{
	const auto build_path = run_tool(cmake()
		.generator(cmake::nmake)
		.root(source_path()));

	run_tool(jom()
		.path(build_path));
}

}	// namespace

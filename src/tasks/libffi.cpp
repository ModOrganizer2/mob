#include "pch.h"
#include "tasks.h"

namespace mob
{

libffi::libffi()
	: basic_task("libffi")
{
}

std::string libffi::version()
{
	return {};
}

bool libffi::prebuilt()
{
	return false;
}

fs::path libffi::source_path()
{
	return paths::build() / "libffi";
}

void libffi::do_fetch()
{
	run_tool(task_conf().make_git()
		.url(make_github_url("python","cpython-bin-deps"))
		.branch("libffi")
		.output(source_path()));
}

fs::path libffi::include_path()
{
	return libffi::source_path() / "amd64" / "include";
}

fs::path libffi::lib_path()
{
	return libffi::source_path() / "amd64";
}

}	// namespace

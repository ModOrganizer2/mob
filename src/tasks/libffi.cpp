#include "pch.h"
#include "tasks.h"

namespace mob
{

libffi::libffi()
	: basic_task("libffi")
{
}

const std::string& libffi::version()
{
	static std::string s;
	return s;
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
	run_tool(git(git::clone_or_pull)
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

#include "pch.h"
#include "tasks.h"

namespace builder
{

libffi::libffi()
	: basic_task("libffi")
{
}

fs::path libffi::source_path()
{
	return paths::build() / "libffi";
}

void libffi::do_fetch()
{
	run_tool(git_clone()
		.org("python")
		.repo("cpython-bin-deps")
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

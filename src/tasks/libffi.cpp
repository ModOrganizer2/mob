#include "pch.h"
#include "tasks.h"

namespace mob::tasks
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
	return conf().path().build() / "libffi";
}

void libffi::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (is_set(c, clean::reclone))
		{
			git_tool::delete_directory(cx(), source_path());
			return;
		}
	});
}

void libffi::do_fetch()
{
	instrument<times::fetch>([&]
	{
		run_tool(task_conf().make_git()
			.url(task_conf().make_git_url("python","cpython-bin-deps"))
			.branch("libffi")
			.root(source_path()));
	});
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

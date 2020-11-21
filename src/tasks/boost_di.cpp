#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

boost_di::boost_di()
	: basic_task("boost-di", "boostdi", "boost_di")
{
}

std::string boost_di::version()
{
	return {};
}

bool boost_di::prebuilt()
{
	return false;
}

fs::path boost_di::source_path()
{
	return conf().path().build() / "di";
}

void boost_di::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (is_set(c, clean::reclone))
		{
			git::delete_directory(cx(), source_path());
			return;
		}
	});
}

void boost_di::do_fetch()
{
	instrument<times::fetch>([&]
	{
		run_tool(task_conf().make_git()
			.url(task_conf().make_git_url("boost-experimental", "di"))
			.branch("cpp14")
			.root(source_path()));
	});
}

}	// namespace

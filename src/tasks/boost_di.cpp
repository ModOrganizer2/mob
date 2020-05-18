#include "pch.h"
#include "tasks.h"

namespace mob
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
	return paths::build() / "di";
}

void boost_di::do_fetch()
{
	run_tool(task_conf().make_git()
		.url(make_github_url("boost-experimental", "di"))
		.branch("cpp14")
		.output(source_path()));
}

}	// namespace

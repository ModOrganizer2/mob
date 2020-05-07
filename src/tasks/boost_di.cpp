
#include "pch.h"
#include "tasks.h"

namespace mob
{

boost_di::boost_di()
	: basic_task("boost-di", "boostdi", "boost_di")
{
}

const std::string& boost_di::version()
{
	static std::string s;
	return s;
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
	run_tool(git_clone()
		.url(make_github_url("boost-experimental", "di"))
		.branch("cpp14")
		.output(source_path()));
}

}	// namespace

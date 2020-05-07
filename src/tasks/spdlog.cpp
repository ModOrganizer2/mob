#include "pch.h"
#include "tasks.h"

namespace mob
{

spdlog::spdlog()
	: basic_task("spdlog")
{
}

const std::string& spdlog::version()
{
	return versions::by_name("spdlog");
}

bool spdlog::prebuilt()
{
	return false;
}

fs::path spdlog::source_path()
{
	return paths::build() / ("spdlog-" + version());
}

void spdlog::do_fetch()
{
	run_tool(git_clone()
		.url(make_github_url("gabime", "spdlog"))
		.branch(version())
		.output(source_path()));
}

}	// namespace

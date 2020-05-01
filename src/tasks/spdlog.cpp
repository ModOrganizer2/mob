#include "pch.h"
#include "tasks.h"

namespace builder
{

spdlog::spdlog()
	: basic_task("spdlog")
{
}

fs::path spdlog::source_path()
{
	return paths::build() / ("spdlog-" + versions::spdlog());
}

void spdlog::do_fetch()
{
	run_tool(git_clone()
		.url(make_github_url("gabime", "spdlog"))
		.branch(versions::spdlog())
		.output(source_path()));
}

}	// namespace

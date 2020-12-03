#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

spdlog::spdlog()
	: basic_task("spdlog")
{
}

std::string spdlog::version()
{
	return conf().version().get("spdlog");
}

bool spdlog::prebuilt()
{
	return false;
}

fs::path spdlog::source_path()
{
	return conf().path().build() / ("spdlog-" + version());
}

void spdlog::do_clean(clean c)
{
	// delete the whole directory
	if (is_set(c, clean::reclone))
		git_wrap::delete_directory(cx(), source_path());
}

void spdlog::do_fetch()
{
	run_tool(make_git()
		.url(make_git_url("gabime", "spdlog"))
		.branch(version())
		.root(source_path()));
}

}	// namespace

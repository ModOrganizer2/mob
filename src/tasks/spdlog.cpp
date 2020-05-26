#include "pch.h"
#include "tasks.h"

namespace mob
{

spdlog::spdlog()
	: basic_task("spdlog")
{
}

std::string spdlog::version()
{
	return conf::version_by_name("spdlog");
}

bool spdlog::prebuilt()
{
	return false;
}

fs::path spdlog::source_path()
{
	return paths::build() / ("spdlog-" + version());
}

void spdlog::do_clean(clean c)
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

void spdlog::do_fetch()
{
	instrument<times::fetch>([&]
	{
		run_tool(task_conf().make_git()
			.url(task_conf().make_git_url("gabime", "spdlog"))
			.branch(version())
			.root(source_path()));
	});
}

}	// namespace

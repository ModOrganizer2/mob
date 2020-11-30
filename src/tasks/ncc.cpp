#include "pch.h"
#include "tasks.h"
#include "../core/process.h"

namespace mob::tasks
{

ncc::ncc()
	: basic_task("ncc")
{
}

std::string ncc::version()
{
	return {};
}

bool ncc::prebuilt()
{
	return false;
}

fs::path ncc::source_path()
{
	return conf().path().build() / "NexusClientCli";
}

void ncc::do_clean(clean c)
{
	if (is_set(c, clean::reclone))
	{
		git_wrap::delete_directory(cx(), source_path());
		return;
	}

	if (is_set(c, clean::rebuild))
		run_tool(create_msbuild_tool(msbuild::clean));
}

void ncc::do_fetch()
{
	run_tool(make_git()
		.url(make_git_url(task_conf().mo_org(), "modorganizer-NCC"))
		.branch(task_conf().mo_branch())
		.root(source_path()));
}

void ncc::do_build_and_install()
{
	run_tool(msbuild()
		.solution(source_path() / "NexusClient.sln")
		.targets({"NexusClientCLI"})
		.platform("Any CPU"));

	const auto publish = source_path() / "publish.bat";

	run_tool(process_runner(process()
		.binary(publish)
		.stderr_level(context::level::trace)
		.arg(conf().path().install_bin())));
}

msbuild ncc::create_msbuild_tool(msbuild::ops o)
{
	return std::move(msbuild(o)
		.solution(source_path() / "NexusClient.sln")
		.targets({"NexusClientCLI"})
		.platform("Any CPU"));
}

}	// namespace

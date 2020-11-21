#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

installer::installer()
	: basic_task("installer", "modorganizer-Installer")
{
}

bool installer::prebuilt()
{
	return false;
}

std::string installer::version()
{
	return {};
}

fs::path installer::source_path()
{
	return modorganizer::super_path() / "installer";
}

void installer::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (is_set(c, clean::reclone))
			git::delete_directory(cx(), source_path());

		if (is_set(c, clean::rebuild))
			op::delete_directory(cx(), conf().path().install_installer());
	});
}

void installer::do_fetch()
{
	const std::string repo = "modorganizer-Installer";

	instrument<times::fetch>([&]
	{
		run_tool(task_conf().make_git()
			.url(task_conf().make_git_url(task_conf().mo_org(), repo))
			.branch(task_conf().mo_branch())
			.root(source_path()));
	});
}

void installer::do_build_and_install()
{
	instrument<times::build>([&]
	{
		run_tool<iscc>(source_path() / "dist" / "MO2-Installer.iss");
	});
}

}	// namespace

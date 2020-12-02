#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

licenses::licenses()
	: basic_task("licenses")
{
}

std::string licenses::version()
{
	return {};
}

bool licenses::prebuilt()
{
	return false;
}

fs::path licenses::source_path()
{
	return {};
}

void licenses::do_build_and_install()
{
	// copy all files from mob's license directory to install/bin/licenses
	op::copy_glob_to_dir_if_better(cx(),
		conf().path().licenses() / "*",
		conf().path().install_licenses(),
		op::copy_files|op::copy_dirs);
}

}	// namespace

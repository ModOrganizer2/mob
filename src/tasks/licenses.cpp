#include "pch.h"
#include "tasks.h"

namespace mob
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
	op::copy_glob_to_dir_if_better(cx(),
		paths::licenses() / "*",
		paths::install_licenses(),
		op::copy_files|op::copy_dirs);
}

}	// namespace

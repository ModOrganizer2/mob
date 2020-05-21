#include "pch.h"
#include "tasks.h"

namespace mob
{

libbsarch::libbsarch()
	: basic_task("libbsarch")
{
}

std::string libbsarch::version()
{
	return conf::version_by_name("libbsarch");
}

bool libbsarch::prebuilt()
{
	return false;
}

fs::path libbsarch::source_path()
{
	return paths::build() / dir_name();
}

void libbsarch::do_fetch()
{
	const auto file = instrument<times::fetch>([&]
	{
		return run_tool(downloader(source_url()));
	});

	instrument<times::extract>([&]
	{
		run_tool(extractor()
			.file(file)
			.output(source_path()));
	});
}

void libbsarch::do_build_and_install()
{
	instrument<times::install>([&]
	{
		op::copy_file_to_dir_if_better(cx(),
			source_path() / "libbsarch.dll",
			paths::install_dlls());
	});
}

std::string libbsarch::dir_name()
{
	return "libbsarch-" + version() + "-release-x64";
}

url libbsarch::source_url()
{
	return
		"https://github.com/ModOrganizer2/libbsarch/releases/download/" +
		version() + "/" + dir_name() + ".7z";
}

}	// namespace

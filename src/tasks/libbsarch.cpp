#include "pch.h"
#include "tasks.h"

namespace mob
{

libbsarch::libbsarch()
	: basic_task("libbsarch")
{
}

fs::path libbsarch::source_path()
{
	return paths::build() / dir_name();
}

void libbsarch::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void libbsarch::do_build_and_install()
{
	op::copy_file_to_dir_if_better(cx(),
		source_path() / "libbsarch.dll",
		paths::install_dlls());
}

std::string libbsarch::dir_name()
{
	return "libbsarch-" + versions::libbsarch() + "-release-x64";
}

url libbsarch::source_url()
{
	return
		"https://github.com/ModOrganizer2/libbsarch/releases/download/" +
		versions::libbsarch() + "/" + dir_name() + ".7z";
}

}	// namespace

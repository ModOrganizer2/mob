#include "pch.h"
#include "tasks.h"

namespace mob
{

libloot::libloot()
	: basic_task("libloot")
{
}

fs::path libloot::source_path()
{
	return paths::build() / dir_name();
}

void libloot::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void libloot::do_build_and_install()
{
	op::copy_file_to_dir_if_better(cx_,
		source_path() / "loot.dll",
		paths::install_loot());
}

std::string libloot::dir_name()
{
	// libloot-0.15.1-0-gf725dd7_0.15.1-win64.7z, yeah
	return
		"libloot-" +
		versions::libloot() + "-" +
		"0-" +
		versions::libloot_hash() + "_" + versions::libloot() + "-" +
		"win64";
}

url libloot::source_url()
{
	return
		"https://github.com/loot/libloot/releases/download/" +
		versions::libloot() + "/" + dir_name() + ".7z";
}

}	// namespace

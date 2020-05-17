#include "pch.h"
#include "tasks.h"

namespace mob
{

libloot::libloot()
	: basic_task("libloot")
{
}

std::string libloot::version()
{
	return conf::version_by_name("libloot");
}

std::string libloot::hash()
{
	return conf::version_by_name("libloot_hash");
}

bool libloot::prebuilt()
{
	return false;
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
	op::copy_file_to_dir_if_better(cx(),
		source_path() / "loot.dll",
		paths::install_loot());
}

std::string libloot::dir_name()
{
	// libloot-0.15.1-0-gf725dd7_0.15.1-win64.7z, yeah
	return
		"libloot-" + version() + "-" + "0-" +
		hash() + "_" + version() + "-" +
		"win64";
}

url libloot::source_url()
{
	return
		"https://github.com/loot/libloot/releases/download/" +
		version() + "/" + dir_name() + ".7z";
}

}	// namespace

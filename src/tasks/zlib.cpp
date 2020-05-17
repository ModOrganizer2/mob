#include "pch.h"
#include "tasks.h"

namespace mob
{

zlib::zlib()
	: basic_task("zlib")
{
}

std::string zlib::version()
{
	return conf::version_by_name("zlib");
}

bool zlib::prebuilt()
{
	return false;
}

fs::path zlib::source_path()
{
	return paths::build() / ("zlib-" + version());
}

void zlib::do_clean_for_rebuild()
{
	cmake::clean(cx(), source_path());
}

void zlib::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void zlib::do_build_and_install()
{
	const auto build_path = run_tool(cmake()
		.generator(cmake::vs)
		.root(source_path())
		.prefix(source_path()));

	run_tool(msbuild()
		.solution(build_path / "INSTALL.vcxproj"));

	op::copy_file_to_dir_if_better(cx(),
		build_path / "zconf.h",
		source_path());
}

url zlib::source_url()
{
	return "https://zlib.net/zlib-" + version() + ".tar.gz";
}

}	// namespace

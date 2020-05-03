#include "pch.h"
#include "tasks.h"

namespace mob
{

bzip2::bzip2()
	: basic_task("bzip2")
{
}

fs::path bzip2::source_path()
{
	return paths::build() / ("bzip2-" + versions::bzip2());
}

void bzip2::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

url bzip2::source_url()
{
	return
		"https://sourceforge.net/projects/bzip2/files/"
		"bzip2-" + versions::bzip2() + ".tar.gz/download";
}

}	// namespace

#include "pch.h"
#include "tasks.h"

namespace mob
{

bzip2::bzip2()
	: basic_task("bzip2")
{
}

std::string bzip2::version()
{
	return conf::version_by_name("bzip2");
}

bool bzip2::prebuilt()
{
	return false;
}

fs::path bzip2::source_path()
{
	return paths::build() / ("bzip2-" + version());
}

void bzip2::do_fetch()
{
	const auto file = instrument(times_.fetch, [&]
	{
		return run_tool(downloader(source_url()));
	});

	instrument(times_.extract, [&]
	{
		run_tool(extractor()
			.file(file)
			.output(source_path()));
	});
}

url bzip2::source_url()
{
	return
		"https://sourceforge.net/projects/bzip2/files/"
		"bzip2-" + version() + ".tar.gz/download";
}

}	// namespace

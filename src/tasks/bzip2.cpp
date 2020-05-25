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

void bzip2::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (is_set(c, clean::redownload))
			run_tool(downloader(source_url(), downloader::clean));

		if (is_set(c, clean::reextract))
		{
			cx().trace(context::reextract, "deleting {}", source_path());
			op::delete_directory(cx(), source_path(), op::optional);
			return;
		}
	});
}

void bzip2::do_fetch()
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

url bzip2::source_url()
{
	return
		"https://sourceforge.net/projects/bzip2/files/"
		"bzip2-" + version() + ".tar.gz/download";
}

}	// namespace

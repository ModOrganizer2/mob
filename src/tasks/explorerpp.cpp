#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

explorerpp::explorerpp()
	: basic_task("explorerpp", "explorer++")
{
}

std::string explorerpp::version()
{
	return conf::version_by_name("explorerpp");
}

bool explorerpp::prebuilt()
{
	return false;
}

fs::path explorerpp::source_path()
{
	return paths::build() / "explorer++";
}

void explorerpp::do_clean(clean c)
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

void explorerpp::do_fetch()
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

	instrument<times::install>([&]
	{
		op::copy_glob_to_dir_if_better(cx(),
			source_path() / "*",
			paths::install_bin() / "explorer++",
			op::copy_files);
	});
}

url explorerpp::source_url()
{
	return
		"https://explorerplusplus.com/software/"
		"explorer++_" + version() + "_x64.zip";
}

}	// namespace

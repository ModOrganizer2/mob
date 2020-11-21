#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

libloot::libloot()
	: basic_task("libloot")
{
}

std::string libloot::version()
{
	return conf().version().get("libloot");
}

std::string libloot::hash()
{
	return conf().version().get("libloot_hash");
}

std::string libloot::branch()
{
	return conf().version().get("libloot_branch");
}

bool libloot::prebuilt()
{
	return false;
}

fs::path libloot::source_path()
{
	return conf().path().build() / release_name();
}

void libloot::do_clean(clean c)
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

void libloot::do_fetch()
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

void libloot::do_build_and_install()
{
	instrument<times::install>([&]
	{
		op::copy_file_to_dir_if_better(cx(),
			source_path() / "loot.dll",
			conf().path().install_loot());
	});
}

std::string libloot::release_name()
{
	// the naming convention is `libloot-version-commit_branch-win64.7z`,
	// such as:
	//
	//  libloot-0.14.6-0-g8fed4b0_dev-win64.7z
	//  libloot-0.15.1-0-gf725dd7_0.15.1-win64.7z
	//  libloot-0.15.2-0-g3baa0e8_master-win64.7z

	return
		"libloot-" + version() + "-" + "0-" +
		hash() + "_" + branch() + "-" +
		"win64";
}

url libloot::source_url()
{
	return
		"https://github.com/loot/libloot/releases/download/" +
		version() + "/" + release_name() + ".7z";
}

}	// namespace

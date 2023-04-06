#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

namespace
{

std::string release_name()
{
	// the naming convention is `libloot-version-win64.7z`,
	// such as libloot-0.19.3-win64.7z.
	return "libloot-" + libloot::version() + "-" + "win64";
}

url source_url()
{
	return
		"https://github.com/loot/libloot/releases/download/" +
		libloot::version() + "/" + release_name() + ".7z";
}

}	// namespace


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
	// delete download
	if (is_set(c, clean::redownload))
		run_tool(downloader(source_url(), downloader::clean));

	// delete the whole directory
	if (is_set(c, clean::reextract))
	{
		cx().trace(context::reextract, "deleting {}", source_path());
		op::delete_directory(cx(), source_path(), op::optional);
	}
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
	// copy dll
	op::copy_file_to_dir_if_better(cx(),
		source_path() / "loot.dll",
		conf().path().install_loot());
}

}	// namespace

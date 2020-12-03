#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

namespace
{

std::string dir_name()
{
	return "libbsarch-" + libbsarch::version() + "-release-x64";
}

url source_url()
{
	return
		"https://github.com/ModOrganizer2/libbsarch/releases/download/" +
		libbsarch::version() + "/" + dir_name() + ".7z";
}

}	// namespace


libbsarch::libbsarch()
	: basic_task("libbsarch")
{
}

std::string libbsarch::version()
{
	return conf().version().get("libbsarch");
}

bool libbsarch::prebuilt()
{
	return false;
}

fs::path libbsarch::source_path()
{
	return conf().path().build() / dir_name();
}

void libbsarch::do_clean(clean c)
{
	// delete the download
	if (is_set(c, clean::redownload))
		run_tool(downloader(source_url(), downloader::clean));

	// delete the whole directory
	if (is_set(c, clean::reextract))
	{
		cx().trace(context::reextract, "deleting {}", source_path());
		op::delete_directory(cx(), source_path(), op::optional);
	}
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
	// copy dll
	op::copy_file_to_dir_if_better(cx(),
		source_path() / "libbsarch.dll",
		conf().path().install_dlls());
}

}	// namespace

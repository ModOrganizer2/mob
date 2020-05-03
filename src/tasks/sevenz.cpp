#include "pch.h"
#include "tasks.h"

namespace mob
{

sevenz::sevenz()
	: basic_task({"7z", "sevenz"})
{
}

fs::path sevenz::source_path()
{
	return paths::build() / ("7zip-" + versions::sevenzip());
}

void sevenz::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(decompresser()
		.file(file)
		.output(source_path()));
}

void sevenz::do_build_and_install()
{
	run_tool(jom()
		.path(module_to_build())
		.def("CPU=x64")
		.def("NEW_COMPILER=1")
		.def("MY_STATIC_LINK=1")
		.def("NO_BUFFEROVERFLOWU=1"));

	op::copy_file_to_dir_if_better(
		module_to_build() / "x64/7z.dll",
		paths::install_dlls());
}

void sevenz::do_clean()
{
	const fs::path out_path = module_to_build() / "x64";

	cx_.log(context::rebuild, "deleting " + out_path.string());
	op::delete_directory(out_path, op::optional, &cx_);
}

url sevenz::source_url()
{
	return "https://www.7-zip.org/a/7z" + version_for_url() + "-src.7z";
}

std::string sevenz::version_for_url()
{
	return replace_all(versions::sevenzip(), ".", "");
}

fs::path sevenz::module_to_build()
{
	return sevenz::source_path() / "CPP" / "7zip" / "Bundles" / "Format7zF";
}

}	// namespace

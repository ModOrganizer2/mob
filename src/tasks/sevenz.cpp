#include "pch.h"
#include "tasks.h"

namespace mob
{

sevenz::sevenz()
	: basic_task("7z", "sevenz")
{
}

const std::string& sevenz::version()
{
	return versions::by_name("sevenz");
}

bool sevenz::prebuilt()
{
	return false;
}

fs::path sevenz::source_path()
{
	return paths::build() / ("7zip-" + version());
}

void sevenz::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void sevenz::do_build_and_install()
{
	build();

	op::copy_file_to_dir_if_better(cx(),
		module_to_build() / "x64/7z.dll",
		paths::install_dlls());
}

void sevenz::build()
{
	const int max_tries = 3;

	for (int tries=0; tries<max_tries; ++tries)
	{
		const int exit_code = run_tool(jom()
			.path(module_to_build())
			.flag(jom::allow_failure)
			.def("CPU=x64")
			.def("NEW_COMPILER=1")
			.def("MY_STATIC_LINK=1")
			.def("NO_BUFFEROVERFLOWU=1"));

		if (exit_code == 0)
			return;

		cx().debug(context::generic,
			"jom /J regularly fails with 7z because of race conditions; "
			"trying again");
	}

	cx().debug(context::generic,
		"jom /J has failed more than " + std::to_string(max_tries) + " "
		"times, restarting one last time without /J; that one should work");

	run_tool(jom()
		.path(module_to_build())
		.def("CPU=x64")
		.def("NEW_COMPILER=1")
		.def("MY_STATIC_LINK=1")
		.def("NO_BUFFEROVERFLOWU=1")
		.flag(jom::single_job));
}

void sevenz::do_clean_for_rebuild()
{
	op::delete_directory(cx(), module_to_build() / "x64", op::optional);
}

url sevenz::source_url()
{
	return "https://www.7-zip.org/a/7z" + version_for_url() + "-src.7z";
}

std::string sevenz::version_for_url()
{
	return replace_all(version(), ".", "");
}

fs::path sevenz::module_to_build()
{
	return sevenz::source_path() / "CPP" / "7zip" / "Bundles" / "Format7zF";
}

}	// namespace

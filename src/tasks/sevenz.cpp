#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

namespace
{

std::string version_for_url()
{
	return replace_all(sevenz::version(), ".", "");
}

url source_url()
{
	return "https://www.7-zip.org/a/7z" + version_for_url() + "-src.7z";
}

// 7z has a bunch of modules, like the gui, etc., just build the dll
//
fs::path module_to_build()
{
	return sevenz::source_path() / "CPP" / "7zip" / "Bundles" / "Format7zF";
}

}	// namespace


sevenz::sevenz()
	: basic_task("7z", "sevenz")
{
}

std::string sevenz::version()
{
	return conf().version().get("sevenz");
}

bool sevenz::prebuilt()
{
	return false;
}

fs::path sevenz::source_path()
{
	return conf().path().build() / ("7zip-" + version());
}

void sevenz::do_clean(clean c)
{
	// delete download
	if (is_set(c, clean::redownload))
		run_tool(downloader(source_url(), downloader::clean));

	// delete whole directory
	if (is_set(c, clean::reextract))
	{
		cx().trace(context::reextract, "deleting {}", source_path());
		op::delete_directory(cx(), source_path(), op::optional);

		// no need to do anything else
		return;
	}

	// delete whole output directory
	if (is_set(c, clean::rebuild))
		op::delete_directory(cx(), module_to_build() / "x64", op::optional);
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

	// copy 7z.dll to install/bin/dlls
	op::copy_file_to_dir_if_better(cx(),
		module_to_build() / "x64/7z.dll",
		conf().path().install_dlls());
}

void sevenz::build()
{
	build_loop(cx(), [&](bool mp)
	{
		const int exit_code = run_tool(jom()
			.path(module_to_build())
			.flag(mp ? jom::allow_failure : jom::single_job)
			.def("CPU=x64")
			.def("NEW_COMPILER=1")
			.def("MY_STATIC_LINK=1")
			.def("NO_BUFFEROVERFLOWU=1"));

		return (exit_code == 0);
	});
}

}	// namespace

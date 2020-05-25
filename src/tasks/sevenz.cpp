#include "pch.h"
#include "tasks.h"

namespace mob
{

sevenz::sevenz()
	: basic_task("7z", "sevenz")
{
}

std::string sevenz::version()
{
	return conf::version_by_name("sevenz");
}

bool sevenz::prebuilt()
{
	return false;
}

fs::path sevenz::source_path()
{
	return paths::build() / ("7zip-" + version());
}

void sevenz::do_clean(clean c)
{
	if (is_set(c, clean::rebuild))
	{
		instrument<times::clean>([&]
		{
			op::delete_directory(cx(),
				module_to_build() / "x64", op::optional);
		});
	}
}

void sevenz::do_fetch()
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

void sevenz::do_build_and_install()
{
	instrument<times::build>([&]
	{
		build();
	});

	instrument<times::install>([&]
	{
		op::copy_file_to_dir_if_better(cx(),
			module_to_build() / "x64/7z.dll",
			paths::install_dlls());
	});
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
		"jom /J has failed more than {} times, "
		"restarting one last time without /J; that one should work",
		max_tries);

	run_tool(jom()
		.path(module_to_build())
		.def("CPU=x64")
		.def("NEW_COMPILER=1")
		.def("MY_STATIC_LINK=1")
		.def("NO_BUFFEROVERFLOWU=1")
		.flag(jom::single_job));
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

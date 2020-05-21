#include "pch.h"
#include "tasks.h"

namespace mob
{

stylesheets::stylesheets()
	: basic_task("ss", "stylesheets")
{
}

std::string stylesheets::version()
{
	return {};
}

std::string stylesheets::paper_lad_6788_version()
{
	return conf::version_by_name("ss_paper_lad_6788");
}

std::string stylesheets::paper_automata_6788_version()
{
	return conf::version_by_name("ss_paper_automata_6788");
}

std::string stylesheets::paper_mono_6788_version()
{
	return conf::version_by_name("ss_paper_mono_6788");
}

std::string stylesheets::dark_mode_1809_6788_version()
{
	return conf::version_by_name("ss_dark_mode_1809_6788");
}


bool stylesheets::prebuilt()
{
	return false;
}

fs::path stylesheets::source_path()
{
	// all projects are dumped in the build directory; this also disables
	// auto patching
	return {};
}

void stylesheets::do_fetch()
{
	instrument<times::fetch>([&]
	{
		// this isn't very generic, but 6788 is the only repo so far

		for (auto&& r : releases())
		{
			const auto file = run_tool(downloader()
				.url(
					"https://github.com/" + r.repo + "/" + r.name + "/releases/"
					"download/v" + r.version + "/" + r.file + ".7z")
				.file(paths::cache() / (r.name + ".7z")));

			run_tool(extractor()
				.file(file)
				.output(paths::build() / (r.name + "-v" + r.version)));
		}
	});
}

void stylesheets::do_build_and_install()
{
	instrument<times::install>([&]
	{
		for (auto&& r : releases())
		{
			const fs::path src = paths::build() / (r.name + "-v" + r.version);

			op::copy_glob_to_dir_if_better(cx(),
				src / "*",
				paths::install_stylesheets(),
				op::copy_files|op::copy_dirs);
		}
	});
}

std::vector<stylesheets::release> stylesheets::releases()
{
	return
	{
		{
			"6788-00",
			"paper-light-and-dark",
			stylesheets::paper_lad_6788_version(),
			stylesheets::paper_lad_6788_version(),
		},

		{
			"6788-00",
			"paper-automata",
			paper_automata_6788_version(),
			"Paper-Automata"
		},

		{
			"6788-00",
			"paper-mono",
			paper_mono_6788_version(),
			"Paper-Mono"
		},

		{
			"6788-00",
			"1809-dark-mode",
			dark_mode_1809_6788_version(),
			dark_mode_1809_6788_version()
		}
	};
}

}	// namespace

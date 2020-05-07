#include "pch.h"
#include "tasks.h"

namespace mob
{

stylesheets::stylesheets()
	: basic_task("ss", "stylesheets")
{
}

const std::string& stylesheets::version()
{
	return {};
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
}

void stylesheets::do_build_and_install()
{
	for (auto&& r : releases())
	{
		const fs::path src = paths::build() / (r.name + "-v" + r.version);

		op::copy_glob_to_dir_if_better(cx(),
			src / "*",
			paths::install_stylesheets(),
			op::copy_files|op::copy_dirs);
	}
}

std::vector<stylesheets::release> stylesheets::releases()
{
	return
	{
		{
			"6788-00",
			"paper-light-and-dark",
			versions::ss_6788_paper_lad(),
			versions::ss_6788_paper_lad(),
		},

		{
			"6788-00",
			"paper-automata",
			versions::ss_6788_paper_automata(),
			"Paper-Automata"
		},

		{
			"6788-00",
			"paper-mono",
			versions::ss_6788_paper_mono(),
			"Paper-Mono"
		},

		{
			"6788-00",
			"1809-dark-mode",
			versions::ss_6788_1809_dark_mode(),
			versions::ss_6788_1809_dark_mode()
		}
	};
}

}	// namespace

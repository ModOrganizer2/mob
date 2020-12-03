#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

stylesheets::stylesheets()
	: task("ss", "stylesheets")
{
}

bool stylesheets::prebuilt()
{
	return false;
}

std::string stylesheets::paper_lad_6788_version()
{
	return conf().version().get("ss_paper_lad_6788");
}

std::string stylesheets::paper_automata_6788_version()
{
	return conf().version().get("ss_paper_automata_6788");
}

std::string stylesheets::paper_mono_6788_version()
{
	return conf().version().get("ss_paper_mono_6788");
}

std::string stylesheets::dark_mode_1809_6788_version()
{
	return conf().version().get("ss_dark_mode_1809_6788");
}

void stylesheets::do_clean(clean c)
{
	// delete download file for each release
	if (is_set(c, clean::redownload))
	{
		for (auto&& r : releases())
			run_tool(make_downloader_tool(r, downloader::clean));
	}

	// delete directory for each release
	if (is_set(c, clean::reextract))
	{
		for (auto&& r : releases())
		{
			const auto p = release_build_path(r);

			cx().trace(context::reextract, "deleting {}", p);
			op::delete_directory(cx(), p, op::optional);
		}
	}
}

void stylesheets::do_fetch()
{
	// download and extract file for each release
	for (auto&& r : releases())
	{
		const auto file = run_tool(make_downloader_tool(r));

		run_tool(extractor()
			.file(file)
			.output(release_build_path(r)));
	}
}

fs::path stylesheets::release_build_path(const release& r) const
{
	// something like build/paper-mono-v2.1
	return conf().path().build() / (r.name + "-v" + r.version);
}

downloader stylesheets::make_downloader_tool(
	const release& r, downloader::ops o) const
{
	// this isn't generic at all and will probably fail for future stylesheets,
	// but 6788 is the only repo so far and that's the convention they use for
	// their releases

	url u =
		"https://github.com/" + r.repo + "/" + r.name + "/releases/"
		"download/v" + r.version + "/" + r.file + ".7z";

	return std::move(downloader(o)
		.url(u)
		.file(conf().path().cache() / (r.name + ".7z")));
}

void stylesheets::do_build_and_install()
{
	for (auto&& r : releases())
	{
		// copy all the files and directories from the source directory directly
		// into install/bin/stylesheets
		op::copy_glob_to_dir_if_better(cx(),
			release_build_path(r) / "*",
			conf().path().install_stylesheets(),
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

#include "pch.h"
#include "tasks.h"

namespace mob::tasks
{

lz4::lz4()
	: basic_task("lz4")
{
}

std::string lz4::version()
{
	return conf().version().get("lz4");
}

bool lz4::prebuilt()
{
	return conf().prebuilt().get<bool>("lz4");
}

fs::path lz4::source_path()
{
	return conf().path().build() / ("lz4-" + version());
}

void lz4::do_clean(clean c)
{
	if (prebuilt())
	{
		if (is_set(c, clean::redownload))
			run_tool(downloader(prebuilt_url(), downloader::clean));

		if (is_set(c, clean::reextract))
		{
			cx().trace(context::reextract, "deleting {}", source_path());
			op::delete_directory(cx(), source_path(), op::optional);
			return;
		}
	}
	else
	{
		if (is_set(c, clean::reclone))
		{
			git_wrap::delete_directory(cx(), source_path());
			return;
		}

		if (is_set(c, clean::rebuild))
			run_tool(create_msbuild_tool(msbuild::clean));
	}
}

void lz4::do_fetch()
{
	if (prebuilt())
		fetch_prebuilt();
	else
		fetch_from_source();
}

void lz4::do_build_and_install()
{
	if (prebuilt())
		build_and_install_prebuilt();
	else
		build_and_install_from_source();
}

void lz4::fetch_prebuilt()
{
	cx().trace(context::generic, "using prebuilt lz4");

	const auto file = run_tool(downloader(prebuilt_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void lz4::build_and_install_prebuilt()
{
	op::copy_file_to_dir_if_better(cx(),
		source_path() / "bin" / "liblz4.pdb",
		conf().path().install_pdbs());

	op::copy_file_to_dir_if_better(cx(),
		source_path() / "bin" / "liblz4.dll",
		conf().path().install_dlls());
}

void lz4::fetch_from_source()
{
	run_tool(task_conf().make_git()
		.url(task_conf().make_git_url("lz4","lz4"))
		.branch(version())
		.root(source_path()));

	run_tool(vs(vs::upgrade)
		.solution(solution_file()));
}

void lz4::build_and_install_from_source()
{
	run_tool(create_msbuild_tool());

	op::copy_glob_to_dir_if_better(cx(),
		out_dir() / "*",
		source_path() / "bin",
		op::copy_files);

	op::copy_file_to_dir_if_better(cx(),
		out_dir() / "liblz4.dll",
		conf().path().install_dlls());

	op::copy_file_to_dir_if_better(cx(),
		out_dir() / "liblz4.pdb",
		conf().path().install_pdbs());
}

msbuild lz4::create_msbuild_tool(msbuild::ops o)
{
	return std::move(msbuild(o)
		.solution(solution_file())
		.targets({"liblz4-dll"}));
}

fs::path lz4::solution_dir()
{
	return source_path() / "visual" / "VS2017";
}

fs::path lz4::solution_file()
{
	return solution_dir() / "lz4.sln";
}

fs::path lz4::out_dir()
{
	return solution_dir() / "bin" / "x64_Release";
}

url lz4::prebuilt_url()
{
	return make_prebuilt_url("lz4_prebuilt_" + version() + ".7z");
}

}	// namespace

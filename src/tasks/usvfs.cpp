#include "pch.h"
#include "tasks.h"

namespace mob
{

usvfs::usvfs()
	: basic_task("usvfs")
{
}

std::string usvfs::version()
{
	return conf::version_by_name("usvfs");
}

bool usvfs::prebuilt()
{
	return conf::prebuilt_by_name("usvfs");
}

fs::path usvfs::source_path()
{
	return paths::build() / "usvfs";
}

void usvfs::do_clean_for_rebuild()
{
	if (prebuilt())
		return;

	instrument<times::clean>([&]
	{
		op::delete_directory(cx(), source_path() / "bin", op::optional);
		op::delete_directory(cx(), source_path() / "lib", op::optional);

		op::delete_directory(cx(),
			source_path() / "vsbuild" / "Release", op::optional);
	});
}

void usvfs::do_fetch()
{
	if (prebuilt())
		fetch_prebuilt();
	else
		fetch_from_source();
}

void usvfs::do_build_and_install()
{
	if (prebuilt())
		build_and_install_prebuilt();
	else
		build_and_install_from_source();
}

void usvfs::fetch_prebuilt()
{
	instrument<times::fetch>([&]
	{
		fetch_from_source();
		download_from_appveyor(arch::x64);
		download_from_appveyor(arch::x86);
	});
}

void usvfs::build_and_install_prebuilt()
{
	instrument<times::install>([&]
	{
		copy_prebuilt(arch::x86);
		copy_prebuilt(arch::x64);
	});
}

void usvfs::fetch_from_source()
{
	instrument<times::fetch>([&]
	{
		run_tool(task_conf().make_git()
			.url(make_github_url(task_conf().mo_org(), "usvfs"))
			.branch(version())
			.root(source_path()));
	});
}

void usvfs::build_and_install_from_source()
{
	instrument<times::build>([&]
	{
		// usvfs doesn't use "Win32" for 32-bit, it uses "x86"
		//
		// note that usvfs_proxy has a custom build step in Release that runs
		// usvfs/vsbuild/stage_helper.cmd, which copies everything into
		// install/

		run_tool(msbuild()
			.platform("x64")
			.projects({"usvfs_proxy"})
			.solution(source_path() / "vsbuild" / "usvfs.sln"));

		run_tool(msbuild()
			.platform("x86")
			.projects({"usvfs_proxy"})
			.solution(source_path() / "vsbuild" / "usvfs.sln"));
	});
}

void usvfs::download_from_appveyor(arch a)
{
	std::string arch_s;
	const std::string dir = prebuilt_directory_name(a);

	switch (a)
	{
		case arch::x86:
			arch_s = "x86";
			break;

		case arch::x64:
			arch_s = "x64";
			break;

		case arch::dont_care:
		default:
			cx().bail_out(context::generic, "bad arch");
	}

	auto dl = [&](std::string filename)
	{
		const auto u = make_appveyor_artifact_url(a, "usvfs", filename);

		return run_tool(downloader()
			.url(u)
			.file(paths::build() / dir / u.filename()));
	};

	parallel(
	{
		{"usvfs", [&]{ dl("lib/usvfs_" + arch_s + ".pdb"); }},
		{"usvfs", [&]{ dl("lib/usvfs_" + arch_s + ".dll"); }},
		{"usvfs", [&]{ dl("lib/usvfs_" + arch_s + ".lib"); }},
		{"usvfs", [&]{ dl("bin/usvfs_proxy_" + arch_s + ".exe"); }},
		{"usvfs", [&]{ dl("bin/usvfs_proxy_" + arch_s + ".pdb"); }}
	});
}

void usvfs::copy_prebuilt(arch a)
{
	const std::string dir = prebuilt_directory_name(a);

	op::copy_glob_to_dir_if_better(cx(),
		paths::build() / dir / "*.pdb",
		paths::install_pdbs(),
		op::copy_files);

	op::copy_glob_to_dir_if_better(cx(),
		paths::build() / dir / "*.lib",
		paths::install_libs(),
		op::copy_files);

	op::copy_glob_to_dir_if_better(cx(),
		paths::build() / dir / "*.dll",
		paths::install_bin(),
		op::copy_files);

	op::copy_glob_to_dir_if_better(cx(),
		paths::build() / dir / "*.exe",
		paths::install_bin(),
		op::copy_files);
}

std::string usvfs::prebuilt_directory_name(arch a)
{
	switch (a)
	{
		case arch::x86:
			return "usvfs_bin_32";

		case arch::x64:
			return "usvfs_bin";

		case arch::dont_care:
		default:
			cx().bail_out(context::generic, "bad arch");
	}
}

}	// namespace

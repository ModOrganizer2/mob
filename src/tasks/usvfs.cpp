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

void usvfs::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (prebuilt())
		{
			if (is_set(c, clean::redownload))
			{
				const auto x86_dls =
					create_appveyor_downloaders(arch::x86, downloader::clean);

				for (auto dl : x86_dls)
					run_tool(*dl);


				const auto x64_dls =
					create_appveyor_downloaders(arch::x64, downloader::clean);

				for (auto dl : x64_dls)
					run_tool(*dl);
			}
		}
		else
		{
			if (is_set(c, clean::rebuild))
			{
				op::delete_directory(cx(), source_path() / "bin", op::optional);
				op::delete_directory(cx(), source_path() / "lib", op::optional);

				run_tool(create_msbuild_tool(arch::x86, msbuild::clean));
				run_tool(create_msbuild_tool(arch::x64, msbuild::clean));
			}
		}
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
			.url(task_conf().make_git_url(task_conf().mo_org(), "usvfs"))
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

		run_tool(create_msbuild_tool(arch::x86));
		run_tool(create_msbuild_tool(arch::x64));
	});
}

void usvfs::download_from_appveyor(arch a)
{
	std::vector<std::pair<std::string, std::function<void ()>>> v;

	for (auto dl : create_appveyor_downloaders(a))
		v.emplace_back("usvfs", [this, dl]{ run_tool(*dl); });

	parallel(v);
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

msbuild usvfs::create_msbuild_tool(arch a, msbuild::ops o) const
{
	// usvfs doesn't use "Win32" for 32-bit, it uses "x86"
	//
	// note that usvfs_proxy has a custom build step in Release that runs
	// usvfs/vsbuild/stage_helper.cmd, which copies everything into
	// install/

	const std::string plat = (a == arch::x64 ? "x64" : "x86");

	return std::move(msbuild(o)
		.platform(plat)
		.targets({"usvfs_proxy"})
		.solution(source_path() / "vsbuild" / "usvfs.sln"));
}

std::vector<std::shared_ptr<downloader>>
usvfs::create_appveyor_downloaders(arch a, downloader::ops o) const
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

	auto make_dl = [&](std::string filename)
	{
		const auto u = make_appveyor_artifact_url(a, "usvfs", filename);

		auto dl = std::make_shared<downloader>(o);

		dl->url(u);
		dl->file(paths::build() / dir / u.filename());

		return dl;
	};

	std::vector<std::shared_ptr<downloader>> v;

	v.push_back(make_dl("lib/usvfs_" + arch_s + ".pdb"));
	v.push_back(make_dl("lib/usvfs_" + arch_s + ".dll"));
	v.push_back(make_dl("lib/usvfs_" + arch_s + ".lib"));
	v.push_back(make_dl("bin/usvfs_proxy_" + arch_s + ".exe"));
	v.push_back(make_dl("bin/usvfs_proxy_" + arch_s + ".pdb"));

	return v;
}

std::string usvfs::prebuilt_directory_name(arch a) const
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

#include "pch.h"
#include "tasks.h"
#include "../core/process.h"

namespace mob::tasks
{

openssl::openssl()
	: basic_task("openssl")
{
}

std::string openssl::version()
{
	return conf::version_by_name("openssl");
}

bool openssl::prebuilt()
{
	return conf().prebuilt().use_prebuilt("openssl");
}

fs::path openssl::source_path()
{
	return conf().paths().build() / ("openssl-" + version());
}

fs::path openssl::build_path()
{
	return source_path() / "build";
}

fs::path openssl::bin_path()
{
	return build_path() / "bin";
}

void openssl::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (prebuilt())
		{
			if (is_set(c, clean::redownload))
				run_tool(downloader(prebuilt_url(), downloader::clean));
		}
		else
		{
			if (is_set(c, clean::redownload))
				run_tool(downloader(source_url(), downloader::clean));
		}

		if (is_any_set(c, clean::reextract|clean::reconfigure|clean::rebuild))
		{
			cx().trace(context::reextract, "deleting {}", source_path());
			op::delete_directory(cx(), source_path(), op::optional);
			return;
		}
	});
}

void openssl::do_fetch()
{
	if (prebuilt())
		fetch_prebuilt();
	else
		fetch_from_source();
}

void openssl::do_build_and_install()
{
	if (prebuilt())
		build_and_install_prebuilt();
	else
		build_and_install_from_source();
}

void openssl::fetch_prebuilt()
{
	cx().trace(context::generic, "using prebuilt openssl");

	const auto file = instrument<times::fetch>([&]
	{
		return run_tool(downloader(prebuilt_url()));
	});

	instrument<times::extract>([&]
	{
		run_tool(extractor()
			.file(file)
			.output(source_path()));
	});
}

void openssl::fetch_from_source()
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

void openssl::build_and_install_prebuilt()
{
	instrument<times::install>([&]
	{
		copy_files();
	});
}

void openssl::build_and_install_from_source()
{
	instrument<times::configure>([&]
	{
		if (fs::exists(source_path() / "makefile"))
			cx().trace(context::bypass, "openssl already configured");
		else
			configure();
	});

	instrument<times::build>([&]
	{
		install_engines();
	});

	instrument<times::install>([&]
	{
		op::copy_file_to_dir_if_better(cx(),
			source_path() / "ms" / "applink.c",
			include_path());

		copy_files();
	});
}

void openssl::configure()
{
	run_tool(process_runner(process()
		.binary(perl::binary())
		.arg("Configure")
		.arg("VC-WIN64A")
		.arg("--openssldir=", build_path())
		.arg("--prefix=", build_path())
		.arg("-FS")
		.arg("-MP1")
		.arg("-wd4566")
		.cwd(source_path())
		.env(env::vs(arch::x64))));
}

void openssl::install_engines()
{
	const int max_tries = 3;

	for (int tries=0; tries<max_tries; ++tries)
	{
		const int exit_code = run_tool(jom()
			.path(source_path())
			.target("install_engines")
			.flag(jom::allow_failure));

		if (exit_code == 0)
			return;

		cx().debug(context::generic,
			"jom /J regularly fails with openssh because of race conditions; "
			"trying again");
	}

	cx().debug(context::generic,
		"jom /J has failed more than {} times, "
		"restarting one last time without /J; that one should work",
		max_tries);

	run_tool(jom()
		.path(source_path())
		.target("install_engines")
		.flag(jom::single_job));
}

void openssl::copy_files()
{
	copy_dlls_to(conf().paths().install_bin());
	copy_pdbs_to(conf().paths().install_pdbs());
}

void openssl::copy_dlls_to(const fs::path& dir)
{
	for (auto&& name : output_names())
	{
		op::copy_file_to_dir_if_better(cx(),
			bin_path() / (name + ".dll"), dir);
	}
}

void openssl::copy_pdbs_to(const fs::path& dir)
{
	for (auto&& name : output_names())
	{
		op::copy_file_to_dir_if_better(cx(),
			bin_path() / (name + ".pdb"), dir);
	}
}

fs::path openssl::include_path()
{
	return openssl::source_path() / "include";
}

url openssl::source_url()
{
	return
		"https://www.openssl.org/source/"
		"openssl-" + version() + ".tar.gz";
}

url openssl::prebuilt_url()
{
	return make_prebuilt_url("openssl-prebuilt-" + version() + ".7z");
}

std::vector<std::string> openssl::output_names()
{
	return
	{
		"libcrypto-" + version_no_patch_underscores() + "-x64",
		"libssl-" + version_no_patch_underscores() + "-x64"
	};
}

openssl::version_info openssl::parsed_version()
{
	// 1.2.3d
	// everything but 1 is optional
	std::regex re(
		"(\\d+)"                        // 1
		"(?:"
			"\\.(\\d+)"                 // .2
			"(?:"
				"\\.(\\d+)([a-zA-Z]+)?" // .3d
			")?"
		")?");

	std::smatch m;

	const auto s = version();

	if (!std::regex_match(s, m, re))
		gcx().bail_out(context::generic, "bad openssl version '{}'", s);

	return {m[1], m[2], m[3]};
}

std::string openssl::version_no_patch_underscores()
{
	auto v = parsed_version();

	std::string s = v.major;

	if (v.minor != "")
		s += "_" + v.minor;

	return s;
}

}	// namespace

#include "pch.h"
#include "tasks.h"
#include "../core/process.h"

namespace mob::tasks
{

namespace
{

url source_url()
{
	return
		"https://www.openssl.org/source/"
		"openssl-" + openssl::version() + ".tar.gz";
}

url prebuilt_url()
{
	return make_prebuilt_url("openssl-prebuilt-" + openssl::version() + ".7z");
}

std::string version_no_patch_underscores()
{
	auto v = openssl::parsed_version();

	std::string s = v.major;

//	if (v.minor != "")
//		s += "_" + v.minor;

	return s;
}

// the filenames of the dlls, but without extension, because this is used for
// both dlls and pdbs
//
std::vector<std::string> output_names()
{
	return
	{
		"libcrypto-" + version_no_patch_underscores() + "-x64",
		"libssl-" + version_no_patch_underscores() + "-x64"
	};
}

}	// namespace


openssl::openssl()
	: basic_task("openssl")
{
}

std::string openssl::version()
{
	return conf().version().get("openssl");
}

bool openssl::prebuilt()
{
	return conf().prebuilt().get<bool>("openssl");
}

fs::path openssl::source_path()
{
	return conf().path().build() / ("openssl-" + version());
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
	if (prebuilt())
	{
		// delete prebuilt download
		if (is_set(c, clean::redownload))
			run_tool(downloader(prebuilt_url(), downloader::clean));
	}
	else
	{
		// delete source download
		if (is_set(c, clean::redownload))
			run_tool(downloader(source_url(), downloader::clean));
	}

	// there's no easy way to clean anything for openssl, it puts files all
	// over the place, just delete the whole thing
	if (is_any_set(c, clean::reextract|clean::reconfigure|clean::rebuild))
	{
		cx().trace(context::reextract, "deleting {}", source_path());
		op::delete_directory(cx(), source_path(), op::optional);
	}
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

	const auto file = run_tool(downloader(prebuilt_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void openssl::fetch_from_source()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void openssl::build_and_install_prebuilt()
{
	// nothing to built for prebuilt, just copy
	copy_files();
}

void openssl::build_and_install_from_source()
{
	// running the Configure perl script generates a file `makefile`; since
	// Configuring takes forever and will fully run every time, don't run it if
	// the makefile already exists
	if (fs::exists(source_path() / "makefile"))
		cx().trace(context::bypass, "openssl already configured");
	else
		configure();

	// run the install_engines target in the makefile, this builds everything
	// required
	install_engines();

	// applink.c is required when building python from source, the .vcxproj
	// assumes it's in the include path for whatever reason, so copy it there
	op::copy_file_to_dir_if_better(cx(),
		source_path() / "ms" / "applink.c",
		include_path());

	copy_files();
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
	build_loop(cx(), [&](bool mp)
	{
		// jom defaults to multiprocess, give allow_failure for multiprocess
		// builds and force single_job for the last single process build

		const int exit_code = run_tool(jom()
			.path(source_path())
			.target("install_engines")
			.flag(mp ? jom::allow_failure : jom::single_job));

		return (exit_code == 0);
	});
}

void openssl::copy_files()
{
	// duplicate the dlls to both bin/ and bin/dlls, they're needed by both
	// MO and Qt
	copy_dlls_to(conf().path().install_bin());
	copy_dlls_to(conf().path().install_dlls());

	// pdbs
	copy_pdbs_to(conf().path().install_pdbs());
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

}	// namespace

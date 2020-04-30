#include "pch.h"
#include "tasks.h"

namespace builder
{

openssl::openssl()
	: basic_task("openssl")
{
}

fs::path openssl::source_path()
{
	return paths::build() / ("openssl-" + versions::openssl());
}

void openssl::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(decompresser()
		.file(file)
		.output(source_path()));
}

void openssl::do_build_and_install()
{
	if (fs::exists(source_path() / "makefile"))
		debug("openssl already configured");
	else
		configure();

	install_engines();
	info("openssl built successfully");

	copy_files();
}

void openssl::configure()
{
	run_tool(process_runner(third_party::perl(), cmd::stdout_is_verbose)
		.arg("Configure")
		.arg("--openssldir=", build_path())
		.arg("--prefix=", build_path())
		.arg("-FS")
		.arg("-MP1")
		.arg("VC-WIN64A")
		.cwd(source_path()));
}

void openssl::install_engines()
{
	for (int tries=0; tries<3; ++tries)
	{
		int exit_code = run_tool(jom()
			.path(source_path())
			.target("install_engines")
			.flag(jom::accept_failure));

		if (exit_code == 0)
			return;
	}

	run_tool(jom()
		.path(source_path())
		.target("install_engines")
		.flag(jom::single_job));
}

void openssl::copy_files()
{
	op::copy_file_to_dir_if_better(
		source_path() / "ms" / "applink.c",
		include_path());

	copy_dlls_to(paths::install_bin());
	copy_dlls_to(paths::install_dlls());
	copy_pdbs_to(paths::install_pdbs());
}

void openssl::copy_dlls_to(const fs::path& dir)
{
	for (auto&& name : output_names())
	{
		op::copy_file_to_dir_if_better(
			build_path() / "bin" / (name + ".dll"), dir);
	}
}

void openssl::copy_pdbs_to(const fs::path& dir)
{
	for (auto&& name : output_names())
	{
		op::copy_file_to_dir_if_better(
			build_path() / "bin" / (name + ".pdb"), dir);
	}
}

fs::path openssl::include_path()
{
	return openssl::source_path() / "include";
}

fs::path openssl::build_path()
{
	return source_path() / "build";
}


url openssl::source_url()
{
	return
		"https://www.openssl.org/source/"
		"openssl-" + versions::openssl() + ".tar.gz";
}

std::vector<std::string> openssl::output_names()
{
	return
	{
		"libcrypto-" + version_no_minor_underscores() + "-x64",
		"libssl-" + version_no_minor_underscores() + "-x64"
	};
}

std::smatch openssl::parse_version()
{
	// 1.1.1d
	// everything but 1 is optional
	std::regex re(R"((\d+)(?:\.(\d+)(?:\.(\d+)([a-zA-Z]+)?)?)?)");
	std::smatch m;

	if (!std::regex_match(versions::openssl(), m, re))
		bail_out("bad openssl version '" + versions::openssl() + "'");

	return m;
}

std::string openssl::version_no_tags()
{
	auto m = parse_version();

	// up to 4 so the tag is skipped if present
	const std::size_t count = std::min<std::size_t>(m.size(), 4);

	std::string s;
	for (std::size_t i=1; i<count; ++i)
	{
		if (!s.empty())
			s += ".";

		s += m[i].str();
	}

	return s;
}

std::string openssl::version_no_minor_underscores()
{
	auto m = parse_version();

	if (m.size() == 2)
		return m[1].str();
	else
		return m[1].str() + "_" + m[2].str();
}

}	// namespace

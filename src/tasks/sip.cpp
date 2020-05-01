#include "pch.h"
#include "tasks.h"

namespace builder
{

sip::sip()
	: basic_task("sip")
{
}

fs::path sip::source_path()
{
	return paths::build() / ("sip-" + versions::sip());
}

void sip::do_fetch()
{
	const auto download_file =
		paths::cache() / ("sip-" + versions::sip() + ".tar.gz");

	if (fs::exists(download_file))
	{
		debug("sip: " + download_file.string() + " already exists");
	}
	else
	{
		run_tool(process_runner(arch::dont_care, python::python_exe(), cmd::noflags)
			.arg("-m", "pip")
			.arg("download")
			.arg("--no-binary=:all:")
			.arg("--no-deps")
			.arg("-d", paths::cache())
			.arg("sip==" + versions::sip()));
	}

	run_tool(decompresser()
		.file(download_file)
		.output(source_path()));
}

void sip::do_build_and_install()
{
	const auto header = source_path() / "sip.h";

	if (fs::exists(header))
	{
		debug("sip.h already exists");
	}
	else
	{
		run_tool(process_runner(arch::dont_care, python::python_exe(), cmd::noflags)
			.arg("setup.py")
			.arg("install")
			.cwd(source_path()));

		const auto sip_module = python::scripts_path() / "sip-module.exe";

		run_tool(process_runner(arch::dont_care, sip_module, cmd::noflags)
			.arg("--sip-h")
			.arg("PyQt5.zip")
			.cwd(source_path()));
	}

	op::copy_file_to_dir_if_better(
		source_path() / "sip.h",
		python::include_path());
}

}	// namespace

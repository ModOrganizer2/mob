#include "pch.h"
#include "tasks.h"

namespace mob
{

sip::sip()
	: basic_task("sip")
{
}

fs::path sip::source_path()
{
	return paths::build() / ("sip-" + versions::sip());
}

fs::path sip::sip_module_exe()
{
	return python::scripts_path() / "sip-module.exe";
}

fs::path sip::sip_install_exe()
{
	return python::scripts_path() / "sip-install.exe";
}

fs::path sip::module_source_path()
{
	// 12.7.2
	// .2 is optional
	std::regex re(R"((\d+)\.(\d+)(?:\.(\d+))?)");
	std::smatch m;

	if (!std::regex_match(versions::pyqt_sip(), m, re))
		bail_out("bad pyqt sip version " + versions::pyqt_sip());

	// 12.7
	const auto dir = m[1].str() + "." + m[2].str();

	return source_path() / "sipbuild" / "module" / "source" / dir;
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
		run_tool(process_runner(process()
			.binary(python::python_exe())
			.arg("-m", "pip")
			.arg("download")
			.arg("--no-binary=:all:")
			.arg("--no-deps")
			.arg("-d", paths::cache())
			.arg("sip==" + versions::sip())));
	}

	run_tool(extractor()
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
		run_tool(process_runner(process()
			.binary(python::python_exe())
			.arg("setup.py")
			.arg("install")
			.cwd(source_path())));


		run_tool(process_runner(process()
			.binary(sip_module_exe())
			.arg("--sip-h")
			.arg("PyQt5.zip")
			.cwd(source_path())));
	}

	op::copy_file_to_dir_if_better(
		source_path() / "sip.h",
		python::include_path());
}

}	// namespace

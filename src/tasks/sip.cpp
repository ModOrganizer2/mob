#include "pch.h"
#include "tasks.h"

namespace mob
{

sip::sip()
	: basic_task("sip")
{
}

const std::string& sip::version()
{
	return versions::by_name("sip");
}

const std::string& sip::version_for_pyqt()
{
	return versions::by_name("pyqt_sip");
}

bool sip::prebuilt()
{
	return false;
}

fs::path sip::source_path()
{
	return paths::build() / ("sip-" + version());
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

	if (!std::regex_match(version_for_pyqt(), m, re))
		bail_out("bad pyqt sip version " + version_for_pyqt());

	// 12.7
	const auto dir = m[1].str() + "." + m[2].str();

	return source_path() / "sipbuild" / "module" / "source" / dir;
}

void sip::do_clean_for_rebuild()
{
	op::delete_directory(cx(), source_path() / "build", op::optional);
}

void sip::do_fetch()
{
	// downloading uses python.exe and so has to wait until it's built
}

void sip::do_build_and_install()
{
	download();

	run_tool(extractor()
		.file(download_file())
		.output(source_path()));

	generate();

	op::copy_file_to_dir_if_better(cx(),
		source_path() / "sip.h",
		python::include_path());
}

void sip::download()
{
	if (fs::exists(download_file()))
	{
		if (conf::redownload())
		{
			cx().trace(context::redownload,
				"deleting " + download_file().string());

			op::delete_file(cx(), download_file(), op::optional);
		}
		else
		{
			cx().trace(context::bypass,
				"sip: " + download_file().string() + " already exists");

			return;
		}
	}

	run_tool(process_runner(process()
		.binary(python::python_exe())
		.arg("-m", "pip")
		.arg("download")
		.arg("--no-binary=:all:")
		.arg("--no-deps")
		.arg("-d", paths::cache())
		.arg("sip==" + version())));
}

void sip::generate()
{
	const auto header = source_path() / "sip.h";

	if (fs::exists(header))
	{
		if (conf::rebuild())
		{
			cx().trace(context::rebuild, "ignoring " + header.string());
		}
		else
		{
			cx().trace(context::bypass, header.string() + " already exists");
			return;
		}
	}

	run_tool(process_runner(process()
		.binary(python::python_exe())
		.stderr_filter([&](process::filter& f)
		{
			if (f.line.find("zip_safe flag not set") != std::string::npos)
				f.lv = context::level::trace;
			else if (f.line.find("module references __file__") != std::string::npos)
				f.lv = context::level::trace;
		})
		.arg("setup.py")
		.arg("install")
		.cwd(source_path())));

	run_tool(process_runner(process()
		.binary(sip_module_exe())
		.arg("--sip-h")
		.arg("PyQt5.zip")
		.cwd(source_path())));
}

fs::path sip::download_file()
{
	return paths::cache() / ("sip-" + version() + ".tar.gz");
}

}	// namespace

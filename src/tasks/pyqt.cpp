#include "pch.h"
#include "tasks.h"
#include "../core/process.h"

// build process for python, sip and pyqt; if one is built from source, all
// three need to be built from source, plus openssl because python needs it
//
//  1) build openssl
//
//  2) build python, needs openssl
//
//  3) build sip, needs python:
//      - download and extract source archive
//      - run `python setup.py install` in sip's directory, this generates
//        sip-install.exe and sip-module.exe in python-XX/Scripts (among others)
//      - run `sip-module.exe --sip-h` to generate a sip.h file in sip's source
//        directory
//      - that header file is copied into python/include and is included by
//        by plugin_python in sipapiaccess.h
//
// 4) build pyqt, needs sip
//      - download and extract source archive
//      - use `pip install` to install PyQt-builder
//      - run `sip-install.exe` with the list of required modules, creating a
//        folder for each module in PyQt5-XX/build/ with `.pyd` files
//      - run `sip-module.exe --sdist`, which creates
//        downloads/PyQt5_sip-XXX.tar.gz
//      - run `pip install` with that file, which creates
//        `python-XX/Lib/site-packages/PyQt5/sip.cp32-win_amd64.pyd`
//      - for installation, a bunch of files from site-packages/PyQt5/ are
//        copied into install/bin/plugins/data/PyQt5, including a .pyi file from
//        sip


namespace mob::tasks
{

namespace
{

url source_url()
{
	return
		"https://pypi.io/packages/source/P/PyQt5/"
		"PyQt5-" + pyqt::version() + ".tar.gz";
}

url prebuilt_url()
{
	return make_prebuilt_url("PyQt5_gpl-prebuilt-" + pyqt::version() + ".7z");
}

// file created by sip-module.exe
//
fs::path sip_install_file()
{
	return "PyQt5_sip-" + sip::version_for_pyqt() + ".tar.gz";
}

std::vector<std::string> modules()
{
	return
	{
		"QtCore",
		"QtGui",
		"QtWidgets",
		"QtOpenGL",
		"QtSvg",
		"_QOpenGLFunctions_2_0",
		"_QOpenGLFunctions_2_1",
		"_QOpenGLFunctions_4_1_Core"
	};
}

}	// namespace


pyqt::pyqt()
	: basic_task("pyqt")
{
}

std::string pyqt::version()
{
	return conf().version().get("pyqt");
}

std::string pyqt::builder_version()
{
	return conf().version().get("pyqt_builder");
}

bool pyqt::prebuilt()
{
	return conf().prebuilt().get<bool>("pyqt");
}

fs::path pyqt::source_path()
{
	return conf().path().build() / ("PyQt5-" + version());
}

fs::path pyqt::build_path()
{
	return source_path() / "build";
}

std::string pyqt::pyqt_sip_module_name()
{
	return "PyQt5.sip";
}

void pyqt::do_clean(clean c)
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

	// delete whole directory
	if (is_set(c, clean::reextract))
	{
		cx().trace(context::reextract, "deleting {}", source_path());
		op::delete_directory(cx(), source_path(), op::optional);

		// no need to do anything else
		return;
	}

	if (!prebuilt())
	{
		// delete the pyqt-sip file that's created when building from source
		if (is_set(c, clean::rebuild))
		{
			op::delete_file(cx(),
				conf().path().cache() / sip_install_file(),
				op::optional);
		}
	}
}

void pyqt::do_fetch()
{
	if (prebuilt())
		fetch_prebuilt();
	else
		fetch_from_source();
}

void pyqt::do_build_and_install()
{
	if (prebuilt())
		build_and_install_prebuilt();
	else
		build_and_install_from_source();
}

void pyqt::fetch_prebuilt()
{
	const auto file = run_tool(downloader(prebuilt_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void pyqt::build_and_install_prebuilt()
{
	// copy the prebuilt files directly into the python directory, they're
	// required by sip, which is always built from source
	op::copy_glob_to_dir_if_better(cx(),
		source_path() / "*",
		python::source_path(),
		op::copy_files|op::copy_dirs);

	// copy files to build/install for MO
	copy_files();
}

void pyqt::fetch_from_source()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void pyqt::build_and_install_from_source()
{
	// use pip to install the pyqt builder
	run_tool(pip(pip::install)
		.package("PyQt-builder")
		.version(builder_version()));

	// patch for builder.py
	run_tool(patcher()
		.task(name())
		.file("builder.py.manual_patch")
		.root(python::site_packages_path() / "pyqtbuild"));

	// build modules and generate the PyQt5_sip-XX.tar.gz file
	sip_build();

	// run pip install for the PyQt5_sip-XX.tar.gz file
	install_sip_file();

	// copy files to build/install for MO
	copy_files();
}

void pyqt::sip_build()
{
	// put qt and python in the path, set CL and LIB, which are used by the
	// visual c++ compiler that's eventually spawned, and set PYTHONHOME
	auto pyqt_env = env::vs_x64()
		.append_path({
			qt::bin_path(),
			python::build_path(),
			python::source_path(),
			python::scripts_path()})
		.set("CL", " /MP")
		.set("LIB", ";" + path_to_utf8(conf().path().install_libs()), env::append)
		.set("PYTHONHOME", path_to_utf8(python::source_path()));

	// create a bypass file, because pyqt always tries to build stuff and it
	// takes forever
	bypass_file built_bypass(cx(), source_path(), "built");

	if (built_bypass.exists())
	{
		cx().trace(context::bypass, "pyqt already built");
	}
	else
	{
		// sip-install.exe has trouble with deleting the build/ directory and
		// trying to recreate it too fast, giving an access denied error; do it
		// here instead
		op::delete_directory(cx(), source_path() / "build", op::optional);

		// build modules
		run_tool(process_runner(process()
			.binary(sip::sip_install_exe())
			.arg("--confirm-license")
			.arg("--verbose", process::log_trace)
			.arg("--pep484-pyi")
			.arg("--link-full-dll")
			.arg("--build-dir", build_path())
			.arg("--enable", "pylupdate")  // these are not in modules so they
			.arg("--enable", "pyrcc")      // don't get copied below
			.args(zip(repeat("--enable"), modules()))
			.cwd(source_path())
			.env(pyqt_env)));

		// done, create the bypass file
		built_bypass.create();
	}

	// generate the PyQt5_sip-XX.tar.gz file
	run_tool(process_runner(process()
		.binary(sip::sip_module_exe())
		.arg("--sdist")
		.arg(pyqt_sip_module_name())
		.cwd(conf().path().cache())
		.env(pyqt_env)));
}

void pyqt::install_sip_file()
{
	// create a bypass file, because pyqt always tries to install stuff and it
	// takes forever
	bypass_file installed_bypass(cx(), source_path(), "installed");

	if (installed_bypass.exists())
	{
		cx().trace(context::bypass, "pyqt already installed");
	}
	else
	{
		// run `pip install` on the generated PyQt5_sip-XX.tar.gz file
		run_tool(pip(pip::install)
			.file(conf().path().cache() / sip_install_file()));

		// done, create the bypass file
		installed_bypass.create();
	}
}

void pyqt::copy_files()
{
	// pyqt puts its files in python-XX/Lib/site-packages/PyQt5
	const fs::path site_packages_pyqt =
		python::site_packages_path() / "PyQt5";

	// target directory is install/bin/plugins/data/PyQt5
	const fs::path pyqt_plugin =
		conf().path().install_plugins() / "data" / "PyQt5";


	// copying a bunch of files from site-packages into the plugins directory

	op::copy_file_to_dir_if_better(cx(),
		site_packages_pyqt / "__init__.py",
		pyqt_plugin);

	op::copy_glob_to_dir_if_better(cx(),
		site_packages_pyqt / "sip*",
		pyqt_plugin,
		op::copy_files);

	for (auto&& m : modules())
	{
		op::copy_file_to_dir_if_better(cx(),
			site_packages_pyqt / (m + ".pyd"),
			pyqt_plugin);

		op::copy_file_to_dir_if_better(cx(),
			site_packages_pyqt / (m + ".pyi"),
			pyqt_plugin,
			op::optional);
	}

	// also copy this from sip's module/source/XX/ directory
	op::copy_file_to_dir_if_better(cx(),
		sip::module_source_path() / "sip.pyi",
		pyqt_plugin);


	// copying some dlls from Qt's installation directory into
	// python-XX/PCBuild/amd64, those are needed by PyQt5 when building several
	// projects

	op::copy_file_to_dir_if_better(cx(),
		qt::bin_path() / "Qt5Core.dll",
		python::build_path(),
		op::unsafe);   // source file is outside prefix

	op::copy_file_to_dir_if_better(cx(),
		qt::bin_path() / "Qt5Xml.dll",
		python::build_path(),
		op::unsafe);   // source file is outside prefix
}

}	// namespace

#include "pch.h"
#include "tasks.h"

namespace mob
{

pyqt::pyqt()
	: basic_task("pyqt")
{
}

fs::path pyqt::source_path()
{
	return paths::build() / ("PyQt5-" + versions::pyqt());
}

void pyqt::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(decompresser()
		.file(file)
		.output(source_path()));
}

void pyqt::do_build_and_install()
{
	const std::vector<std::string> modules =
	{
		"QtCore",
		"QtGui",
		"QtWidgets",
		"QtOpenGL",
		"_QOpenGLFunctions_2_0",
		"_QOpenGLFunctions_2_1",
		"_QOpenGLFunctions_4_1_Core"
	};

	run_tool(pip_install()
		.package("PyQt-builder")
		.version(versions::pyqt_builder()));

	run_tool(patcher()
		.task(name())
		.file("builder.py.manual_patch")
		.root(python::site_packages_path() / "pyqtbuild"));

	auto pyqt_env = env::vs_x64()
		.append_path({
			paths::qt_bin(),
			python::build_path(),
			python::source_path(),
			python::scripts_path()})
		.set("CL", " /MP")
		.set("LIB", ";" + paths::install_libs().string(), env::append)
		.set("PYTHONHOME", python::source_path().string());

	if (fs::exists(source_path() / "_mob_built"))
	{
		debug("pyqt already built");
	}
	else
	{
		// sip-install.exe has trouble with deleting the build/ directory and
		// trying to recreate it to fast, giving an access denied error; do it
		// here instead
		if (fs::exists(source_path() / "build"))
			op::delete_directory(source_path() / "build");

		run_tool(process_runner(process()
			.binary(sip::sip_install_exe())
			.arg("--confirm-license")
			.arg("--verbose")
			.arg("--pep484-pyi")
			.arg("--link-full-dll")
			.arg("--build-dir", build_dir())
			.arg("--enable", "pylupdate")  // these are not in modules so they
			.arg("--enable", "pyrcc")      // don't get copied below
			.args(zip(repeat("--enable"), modules))
			.cwd(source_path())
			.env(pyqt_env)));

		op::touch(source_path() / "_mob_built");
	}

	run_tool(process_runner(process()
		.binary(sip::sip_module_exe())
		.arg("--sdist")
		.arg("PyQt5.sip")
		.cwd(paths::cache())
		.env(pyqt_env)));

	if (fs::exists(source_path() / "_mob_installed"))
	{
		debug("pyqt already installed");
	}
	else
	{
		run_tool(pip_install()
			.file(paths::cache() / sip_install_file()));

		op::touch(source_path() / "_mob_installed");
	}


	const fs::path site_packages_pyqt = python::site_packages_path() / "PyQt5";
	const fs::path pyqt_plugin = paths::install_plugins() / "data" / "PyQt5";

	op::copy_file_to_dir_if_better(
		site_packages_pyqt / "__init__.py",
		pyqt_plugin);

	op::copy_file_to_dir_if_better(
		site_packages_pyqt / "sip*",
		pyqt_plugin);

	for (auto&& m : modules)
	{
		op::copy_file_to_dir_if_better(
			site_packages_pyqt / (m + ".pyd"),
			pyqt_plugin);

		op::copy_file_to_dir_if_better(
			site_packages_pyqt / (m + ".pyi"),
			pyqt_plugin,
			op::optional);
	}

	op::copy_file_to_dir_if_better(
		sip::module_source_path() / "sip.pyi",
		pyqt_plugin);

}

url pyqt::source_url()
{
	return
		"https://pypi.io/packages/source/P/PyQt5/"
		"PyQt5-" + versions::pyqt() + ".tar.gz";
}

fs::path pyqt::build_dir()
{
	return source_path() / "build";
}

fs::path pyqt::sip_install_file()
{
	return "PyQt5_sip-" + versions::pyqt_sip() + ".tar.gz";
}

}	// namespace

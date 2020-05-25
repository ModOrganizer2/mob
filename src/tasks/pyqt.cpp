#include "pch.h"
#include "tasks.h"

namespace mob
{

pyqt::pyqt()
	: basic_task("pyqt")
{
}

std::string pyqt::version()
{
	return conf::version_by_name("pyqt");
}

std::string pyqt::builder_version()
{
	return conf::version_by_name("pyqt_builder");
}

bool pyqt::prebuilt()
{
	return conf::prebuilt_by_name("pyqt");
}

fs::path pyqt::source_path()
{
	return paths::build() / ("PyQt5-" + version());
}

fs::path pyqt::build_path()
{
	return source_path() / "build";
}

void pyqt::do_clean(clean c)
{
	if (prebuilt())
		return;

	if (is_set(c, clean::rebuild))
	{
		instrument<times::clean>([&]
		{
			op::delete_file(cx(),
				paths::cache() / sip_install_file(),
				op::optional);
		});
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

void pyqt::build_and_install_prebuilt()
{
	instrument<times::install>([&]
	{
		op::copy_glob_to_dir_if_better(cx(),
			source_path() / "*",
			python::source_path(),
			op::copy_files|op::copy_dirs);

		copy_files();
	});
}

void pyqt::fetch_from_source()
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

void pyqt::build_and_install_from_source()
{
	instrument<times::build>([&]
	{
		run_tool(pip_install()
			.package("PyQt-builder")
			.version(builder_version()));

		run_tool(patcher()
			.task(name())
			.file("builder.py.manual_patch")
			.root(python::site_packages_path() / "pyqtbuild"));

		sip_build();
	});

	instrument<times::install>([&]
	{
		install_sip_file();
		copy_files();
	});
}

void pyqt::sip_build()
{
	auto pyqt_env = env::vs_x64()
		.append_path({
			qt::bin_path(),
			python::build_path(),
			python::source_path(),
			python::scripts_path()})
		.set("CL", " /MP")
		.set("LIB", ";" + path_to_utf8(paths::install_libs()), env::append)
		.set("PYTHONHOME", path_to_utf8(python::source_path()));


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

		built_bypass.create();
	}

	run_tool(process_runner(process()
		.binary(sip::sip_module_exe())
		.arg("--sdist")
		.arg("PyQt5.sip")
		.cwd(paths::cache())
		.env(pyqt_env)));
}

void pyqt::install_sip_file()
{
	bypass_file installed_bypass(cx(), source_path(), "installed");

	if (installed_bypass.exists())
	{
		cx().trace(context::bypass, "pyqt already installed");
	}
	else
	{
		run_tool(pip_install()
			.file(paths::cache() / sip_install_file()));

		installed_bypass.create();
	}
}

void pyqt::copy_files()
{
	const fs::path site_packages_pyqt = python::site_packages_path() / "PyQt5";
	const fs::path pyqt_plugin = paths::install_plugins() / "data" / "PyQt5";

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

	op::copy_file_to_dir_if_better(cx(),
		sip::module_source_path() / "sip.pyi",
		pyqt_plugin);

	// these are needed by PyQt5 while building several projects

	op::copy_file_to_dir_if_better(cx(),
		qt::bin_path() / "Qt5Core.dll",
		python::build_path(),
		op::unsafe);   // source file is outside prefix

	op::copy_file_to_dir_if_better(cx(),
		qt::bin_path() / "Qt5Xml.dll",
		python::build_path(),
		op::unsafe);   // source file is outside prefix
}

url pyqt::source_url()
{
	return
		"https://pypi.io/packages/source/P/PyQt5/"
		"PyQt5-" + version() + ".tar.gz";
}

url pyqt::prebuilt_url()
{
	return make_prebuilt_url("PyQt5_gpl-prebuilt-" + version() + ".7z");
}

fs::path pyqt::sip_install_file()
{
	return "PyQt5_sip-" + sip::version_for_pyqt() + ".tar.gz";
}

std::vector<std::string> pyqt::modules()
{
	return
	{
		"QtCore",
		"QtGui",
		"QtWidgets",
		"QtOpenGL",
		"_QOpenGLFunctions_2_0",
		"_QOpenGLFunctions_2_1",
		"_QOpenGLFunctions_4_1_Core"
	};
}

}	// namespace

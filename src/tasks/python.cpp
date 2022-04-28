#include "pch.h"
#include "tasks.h"
#include "../core/process.h"

// see the top of pyqt.cpp for some stuff about python/sip/pyqt

namespace mob::tasks
{

namespace
{

std::string version_without_v()
{
	const auto v = python::parsed_version();

	// 3.8.1
	std::string s = v.major + "." + v.minor;
	if (v.patch != "")
		s += "." + v.patch;

	return s;
}

std::string version_for_dll()
{
	const auto v = python::parsed_version();

	// 38
	return v.major + v.minor;
}

url prebuilt_url()
{
	return make_prebuilt_url("python-prebuilt-" + version_without_v() + ".7z");
}

fs::path solution_file()
{
	return python::source_path() / "PCBuild" / "pcbuild.sln";
}

fs::path python_core_zip_file()
{
	return
		python::build_path() /
		"pythoncore" /
		("python" + version_for_dll() + ".zip");
}

}	// namespace


python::python()
	: basic_task("python")
{
}

std::string python::version()
{
	return conf().version().get("python");
}

bool python::prebuilt()
{
	return conf().prebuilt().get<bool>("python");
}

python::version_info python::parsed_version()
{
	// v3.8.1
	// v and .1 are optional
	std::regex re(R"(v?(\d+)\.(\d+)(?:\.(\d+))?)");
	std::smatch m;

	const auto s = version();

	if (!std::regex_match(s, m, re))
		gcx().bail_out(context::generic, "bad python version '{}'", s);

	version_info v;

	v.major = m[1];
	v.minor = m[2];
	v.patch = m[3];

	return v;
}

fs::path python::source_path()
{
	return conf().path().build() / ("python-" + version_without_v());
}

fs::path python::build_path()
{
	return source_path() / "PCBuild" / "amd64";
}

void python::do_clean(clean c)
{
	if (prebuilt())
	{
		// delete download
		if (is_set(c, clean::redownload))
			run_tool(downloader(prebuilt_url(), downloader::clean));

		// delete the whole directory
		if (is_set(c, clean::reextract))
		{
			cx().trace(context::reextract, "deleting {}", source_path());
			op::delete_directory(cx(), source_path(), op::optional);
		}
	}
	else
	{
		// delete the whole directory
		if (is_set(c, clean::reclone))
		{
			git_wrap::delete_directory(cx(), source_path());

			// no need to do anything else
			return;
		}

		// msbuild clean
		if (is_set(c, clean::rebuild))
			run_tool(create_msbuild_tool(msbuild::clean));
	}
}

void python::do_fetch()
{
	if (prebuilt())
		fetch_prebuilt();
	else
		fetch_from_source();
}

void python::do_build_and_install()
{
	if (prebuilt())
		build_and_install_prebuilt();
	else
		build_and_install_from_source();
}

void python::fetch_prebuilt()
{
	const auto file = run_tool(downloader(prebuilt_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void python::build_and_install_prebuilt()
{
	install_pip();
	copy_files();
}

void python::fetch_from_source()
{
	run_tool(make_git()
		.url(make_git_url("python", "cpython"))
		.branch(version())
		.root(source_path()));
}

void python::build_and_install_from_source()
{
	// build
	run_tool(create_msbuild_tool());

	// package stuff into pythoncore.zip
	package();

	// install pip for other tasks that need it
	install_pip();

	// boost.python expects pyconfig.h to be in the include path
	op::copy_file_to_dir_if_better(cx(),
		source_path() / "PC" / "pyconfig.h",
		include_path());

	copy_files();
}

void python::package()
{
	if (fs::exists(python_core_zip_file()))
	{
		cx().trace(context::bypass, "python already packaged");
		return;
	}

	const auto bat = source_path() / "python.bat";

	// package libs into pythonXX.zip
	run_tool(process_runner(process()
		.binary(bat)
		.arg(fs::path("PC/layout"))
		.arg("--source", source_path())
		.arg("--build", build_path())
		.arg("--temp", (build_path() / "pythoncore_temp"))
		.arg("--copy", (build_path() / "pythoncore"))
		.arg("--preset-embed")
		.cwd(source_path())));
}

void python::copy_files()
{
	// libs
	op::copy_glob_to_dir_if_better(cx(),
		build_path() / "*.lib",
		conf().path().install_libs(),
		op::copy_files);

	// pdbs
	op::copy_file_to_dir_if_better(cx(),
		build_path() / ("python" + version_for_dll() + ".pdb"),
		conf().path().install_pdbs());

	// dlls and python libraries are installed by the python plugin
}

void python::install_pip()
{
	cx().trace(context::generic, "installing pip");
	run_tool(pip(pip::ensure));
}

msbuild python::create_msbuild_tool(msbuild::ops o)
{
	return std::move(msbuild(o)
		.solution(solution_file())
		.targets({
			"python", "pythonw", "python3dll", "select", "pyexpat",
			"unicodedata", "_queue", "_bz2", "_ssl", "_overlapped"})
		.properties({
			"bz2Dir=" + path_to_utf8(bzip2::source_path()),
			"zlibDir=" + path_to_utf8(zlib::source_path()),
			"opensslIncludeDir=" + path_to_utf8(openssl::include_path()),
			"opensslOutDir=" + path_to_utf8(openssl::source_path()),
			"libffiIncludeDir=" + path_to_utf8(libffi::include_path()),
			"libffiOutDir=" + path_to_utf8(libffi::lib_path())}));
}

fs::path python::python_exe()
{
	return build_path() / "python.exe";
}

fs::path python::include_path()
{
	return source_path() / "Include";
}

fs::path python::scripts_path()
{
	return source_path() / "Scripts";
}

fs::path python::site_packages_path()
{
	return source_path() / "Lib" / "site-packages";
}

}	// namespace

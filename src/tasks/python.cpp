#include "pch.h"
#include "tasks.h"

namespace mob
{

python::python()
	: basic_task("python")
{
}

const std::string& python::version()
{
	return versions::by_name("python");
}

bool python::prebuilt()
{
	return prebuilt::by_name("python");
}

python::version_info python::parsed_version()
{
	// v3.8.1
	// v and .1 are optional
	std::regex re(R"(v?(\d+)\.(\d+)(?:\.(\d+))?)");
	std::smatch m;

	if (!std::regex_match(version(), m, re))
		bail_out("bad python version '" + version() + "'");

	version_info v;

	v.major = m[1];
	v.minor = m[2];

	if (m.size() > 3)
		v.patch = m[3];

	return v;
}

std::string python::version_without_v()
{
	const auto v = parsed_version();

	// 3.8.1
	std::string s = v.major + "." + v.minor;
	if (v.patch != "")
		s += "." + v.patch;

	return s;
}

fs::path python::source_path()
{
	return paths::build() / ("python-" + version_without_v());
}

fs::path python::build_path()
{
	return source_path() / "PCBuild" / "amd64";
}

void python::do_clean_for_rebuild()
{
	if (prebuilt())
		return;

	const fs::path pcbuild = source_path() / "PCBuild";

	op::delete_directory(cx(), pcbuild / "amd64", op::optional);
	op::delete_directory(cx(), pcbuild / "obj", op::optional);
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
	op::copy_glob_to_dir_if_better(cx(),
		openssl::bin_path() / "*.dll",
		python::build_path(),
		op::copy_files);

	install_pip();
	copy_files();
}

void python::fetch_from_source()
{
	run_tool(git_clone()
		.url(make_github_url("python", "cpython"))
		.branch(version())
		.output(source_path()));

	run_tool(devenv_upgrade(solution_file()));
}

void python::build_and_install_from_source()
{
	run_tool(msbuild()
		.solution(solution_file())
		.projects({
			"python", "pythonw", "python3dll", "select", "pyexpat",
			"unicodedata", "_queue", "_bz2", "_ssl"})
			.parameters({
			"bz2Dir=" + bzip2::source_path().string(),
			"zlibDir=" + zlib::source_path().string(),
			"opensslIncludeDir=" + openssl::include_path().string(),
			"opensslOutDir=" + openssl::source_path().string(),
			"libffiIncludeDir=" + libffi::include_path().string(),
			"libffiOutDir=" + libffi::lib_path().string()}));

	package();
	install_pip();

	op::copy_file_to_dir_if_better(cx(),
		source_path() / "PC" / "pyconfig.h",
		include_path());

	copy_files();
}

void python::package()
{
	bypass_file packaged_bypass(cx(), build_path(), "packaged");

	if (packaged_bypass.exists())
	{
		cx().trace(context::bypass, "python already packaged");
	}
	else
	{
		const auto bat = source_path() / "python.bat";

		run_tool(process_runner(process()
			.binary(bat)
			.arg(fs::path("PC/layout"))
			.arg("--source", source_path())
			.arg("--build", build_path())
			.arg("--temp", (build_path() / "pythoncore_temp"))
			.arg("--copy", (build_path() / "pythoncore"))
			.arg("--preset-embed")
			.cwd(source_path())));

		op::touch(cx(), build_path() / "_mob_packaged");
	}
}

void python::copy_files()
{
	op::copy_glob_to_dir_if_better(cx(),
		build_path() / "*.lib",
		paths::install_libs(),
		op::copy_files);

	op::copy_glob_to_dir_if_better(cx(),
		build_path() / "libffi*.dll",
		paths::install_bin(),
		op::copy_files);

	op::copy_file_to_dir_if_better(cx(),
		build_path() / ("python" + version_for_dll() + ".dll"),
		paths::install_bin());

	op::copy_file_to_dir_if_better(cx(),
		build_path() / ("python" + version_for_dll() + ".pdb"),
		paths::install_pdbs());

	op::copy_glob_to_dir_if_better(cx(),
		build_path() / "pythoncore/*.pyd",
		paths::install_pythoncore(),
		op::copy_files);

	op::copy_file_to_file_if_better(cx(),
		build_path() / "pythoncore" / ("python" + version_for_dll() + ".zip"),
		paths::install_bin() / "pythoncore.zip",
		op::copy_files);
}

void python::install_pip()
{
	cx().trace(context::generic, "installing pip");

	run_tool(process_runner(process()
		.binary(python_exe())
		.arg("-m", "ensurepip")));

	run_tool(process_runner(process()
		.binary(python_exe())
		.arg("-m pip")
		.arg("install")
		.arg("--upgrade pip")));
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

url python::prebuilt_url()
{
	return make_prebuilt_url("python-prebuilt-" + version_without_v() + ".7z");
}

fs::path python::solution_file()
{
	return source_path() / "PCBuild" / "pcbuild.sln";
}

std::string python::version_for_dll()
{
	const auto v = parsed_version();

	// 38
	return v.major + v.minor;
}

}	// namespace

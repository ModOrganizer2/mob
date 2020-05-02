#include "pch.h"
#include "tasks.h"

namespace mob
{

python::python()
	: basic_task("python")
{
}

python::version_info python::version()
{
	// v3.8.1
	// v and .1 are optional
	std::regex re(R"(v?(\d+)\.(\d+)(?:\.(\d+))?)");
	std::smatch m;

	if (!std::regex_match(versions::python(), m, re))
		bail_out("bad python version '" + versions::python() + "'");

	version_info v;

	v.major = m[1];
	v.minor = m[2];

	if (m.size() > 3)
		v.patch = m[3];

	return v;
}

fs::path python::source_path()
{
	const auto v = version();

	// 3.8.1
	std::string s = v.major + "." + v.minor;
	if (v.patch != "")
		s += "." + v.patch;

	return paths::build() / ("python-" + s);
}

fs::path python::build_path()
{
	return source_path() / "PCBuild" / "amd64";
}

void python::do_fetch()
{
	run_tool(git_clone()
		.url(make_github_url("python", "cpython"))
		.branch(versions::python())
		.output(source_path()));

	run_tool(devenv_upgrade(solution_file()));
}

void python::do_build_and_install()
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

	if (fs::exists(build_path() / "_mob_packaged"))
	{
		debug("python already packaged");
	}
	else
	{
		const auto bat = source_path() / "python.bat";

		run_tool(process_runner(process()
			.binary(bat)
			.flags(process::stdout_is_verbose)
			.arg(fs::path("PC/layout"))
			.arg("--source", source_path())
			.arg("--build", build_path())
			.arg("--temp", (build_path() / "pythoncore_temp"))
			.arg("--copy", (build_path() / "pythoncore"))
			.arg("--preset-embed")
			.cwd(source_path())));

		op::touch(build_path() / "_mob_packaged");
	}

	install_pip();

	op::copy_file_to_dir_if_better(
		source_path() / "PC" / "pyconfig.h",
		include_path());

	op::copy_file_to_dir_if_better(
		build_path() / "*.lib",
		paths::install_libs());

	op::copy_file_to_dir_if_better(
		build_path() / "libffi*.dll",
		paths::install_bin());

	op::copy_file_to_dir_if_better(
		build_path() / ("python" + version_for_dll() + ".dll"),
		paths::install_bin());

	op::copy_file_to_dir_if_better(
		build_path() / ("python" + version_for_dll() + ".pdb"),
		paths::install_pdbs());
}

void python::install_pip()
{
	debug("installing pip");

	run_tool(process_runner(process()
		.binary(python_exe())
		.arg("-m", "ensurepip")));
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

fs::path python::solution_file()
{
	return source_path() / "PCBuild" / "pcbuild.sln";
}

std::string python::version_for_dll()
{
	const auto v = version();

	// 38
	return v.major + v.minor;
}

}	// namespace

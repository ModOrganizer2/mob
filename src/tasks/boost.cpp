#include "pch.h"
#include "tasks.h"

namespace builder
{

boost::boost()
	: basic_task("boost")
{
}

fs::path boost::source_path()
{
	return paths::build() / ("boost_" + boost_version_no_tags_underscores());
}

void boost::do_fetch()
{
	if (prebuilt::boost())
		fetch_prebuilt();
	else
		fetch_from_source();
}

void boost::do_build_and_install()
{
	if (prebuilt::boost())
		build_and_install_prebuilt();
	else
		build_and_install_from_source();
}

void boost::fetch_prebuilt()
{
	const auto file = run_tool(downloader(prebuilt_url()));

	run_tool(decompresser()
		.file(file)
		.output(source_path()));
}

void boost::build_and_install_prebuilt()
{
	op::copy_file_to_dir_if_better(
		lib_path() / python_dll(), paths::install_bin());
}

void boost::fetch_from_source()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(decompresser()
		.file(file)
		.output(source_path()));

	if (fs::exists(source_path() / "b2.exe"))
	{
		debug("boost already bootstraped");
	}
	else
	{
		write_config_jam();

		run_tool(process_runner(source_path() / "bootstrap.bat", cmd::noflags)
			.cwd(source_path()));
	}
}

void boost::build_and_install_from_source()
{
	const fs::path out = "lib64-msvc-" + versions::vs_toolset();

	run_tool(process_runner(source_path() / "b2", cmd::noflags)
		.arg("address-model=", "64")
		//.arg("-a")  // rebuild all
		.arg("link=", "shared")
		.arg("--user-config=", config_jam_file())
		.arg("toolset=msvc-" + versions::vs_toolset())
		.arg("--stagedir=", out)
		.arg("--libdir=", out)
		.arg("--with-python")
		.cwd(source_path()));

	op::copy_file_to_dir_if_better(
		source_path() / out / "lib" / python_dll(),
		paths::install_bin());
}

void boost::write_config_jam()
{
	std::ofstream out(config_jam_file());

	out
		<< "using python\n"
		<< "  : " << python_version_for_jam() << "\n"
		<< "  : " << python::python_exe().generic_string() << "\n"
		<< "  : " << python::include_path().generic_string() << "\n"
		<< "  : " << python::build_path().generic_string() << "\n"
		<< "  : <address-model>64\n"
		<< "  : <define>BOOST_ALL_NO_LIB=1\n"
		<< "  ;";
}


std::smatch boost::parse_boost_version()
{
	// 1.72.0-b1-rc1
	// everything but 1.72 is optional
	std::regex re(R"((\d+)\.(\d+)(?:\.(\d+)(?:-(\w+)(?:-(\w+))?)?)?)");
	std::smatch m;

	if (!std::regex_match(versions::boost(), m, re))
		bail_out("bad boost version '" + versions::boost() + "'");

	return m;
}

std::string boost::source_download_filename()
{
	return boost_version_all_underscores() + ".zip";
}

fs::path boost::config_jam_file()
{
	return source_path() / "user-config-64.jam";
}

url boost::prebuilt_url()
{
	const auto underscores = replace_all(versions::boost(), ".", "_");

	return
		"https://github.com/ModOrganizer2/modorganizer-umbrella/"
		"releases/download/1.1/boost_prebuilt_" + underscores + ".7z";
}

url boost::source_url()
{
	return
		"https://dl.bintray.com/boostorg/release/" +
		boost_version_no_tags() + "/source/" +
		boost_version_all_underscores() + ".zip";
}

fs::path boost::lib_path()
{
	const std::string lib = "lib64-msvc-" + versions::boost_vs();
	return source_path() / lib / "lib";
}

std::string boost::python_dll()
{
	std::ostringstream oss;

	// builds something like boost_python38-vc142-mt-x64-1_72.dll

	// boost_python38-
	oss << "boost_python" << python_version_for_dll() + "-";

	// vc142-
	oss << "vc" + replace_all(versions::boost_vs(), ".", "") << "-";

	// mt-x64-1_72
	oss << "mt-x64-" << boost_version_no_patch_underscores();

	oss << ".dll";

	return oss.str();
}

std::string boost::python_version_for_dll()
{
	const auto v = python::version();

	// 38
	return v.major + v.minor;
}

std::string boost::python_version_for_jam()
{
	const auto v = python::version();

	// 3.8
	return v.major + "." + v.minor;
}

std::string boost::boost_version_no_patch_underscores()
{
	const auto m = parse_boost_version();

	// 1_72
	return m[1].str() + "_" + m[2].str();
}

std::string boost::boost_version_no_tags()
{
	const auto m = parse_boost_version();

	// 1.72.1
	std::string s = m[1].str() + "." + m[2].str();

	if (m.size() > 3)
		s += "." + m[3].str();

	return s;
}

std::string boost::boost_version_no_tags_underscores()
{
	return replace_all(boost_version_no_tags(), ".", "_");
}

std::string boost::boost_version_all_underscores()
{
	const auto m = parse_boost_version();

	// boost_1_72_0_b1_rc1
	std::string s = "boost_" + m[1].str() + "_" + m[2].str();

	if (m.size() > 3)
		s += "_" + m[3].str();

	if (m.size() > 4)
		s += "_" + m[4].str();

	if (m.size() > 5)
		s += "_" + m[5].str();

	return s;
}

}	// namespace

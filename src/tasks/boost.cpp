#include "pch.h"
#include "tasks.h"
#include "../core/process.h"

namespace mob::tasks
{

boost::boost()
	: basic_task("boost")
{
}

std::string boost::version()
{
	return conf().version().get("boost");
}

std::string boost::version_vs()
{
	return conf().version().get("boost_vs");
}

bool boost::prebuilt()
{
	return conf().prebuilt().get<bool>("boost");
}

fs::path boost::source_path()
{
	return
		conf().path().build() /
		("boost_" + boost_version_no_tags_underscores());
}

fs::path boost::lib_path(arch a)
{
	return root_lib_path(a) / "lib";
}

fs::path boost::root_lib_path(arch a)
{
	const std::string lib =
		"lib" + address_model_for_arch(a) + "-msvc-" + version_vs();

	return source_path() / lib;
}

void boost::do_clean(clean c)
{
	if (prebuilt())
	{
		if (is_set(c, clean::redownload))
			run_tool(downloader(prebuilt_url(), downloader::clean));
	}
	else
	{
		if (is_set(c, clean::redownload))
			run_tool(downloader(source_url(), downloader::clean));
	}


	if (is_set(c, clean::reextract))
	{
		cx().trace(context::reextract, "deleting {}", source_path());
		op::delete_directory(cx(), source_path(), op::optional);
		return;
	}


	if (!prebuilt())
	{
		if (is_set(c, clean::reconfigure))
		{
			op::delete_directory(cx(), source_path() / "bin.v2", op::optional);
			op::delete_file(cx(), b2_exe(), op::optional);
		}

		if (is_set(c, clean::rebuild))
		{
			op::delete_directory(cx(), root_lib_path(arch::x86), op::optional);
			op::delete_directory(cx(), root_lib_path(arch::x64), op::optional);
			op::delete_file(cx(), config_jam_file(), op::optional);
			op::delete_file(cx(), source_path() / "project-config.jam", op::optional);
		}
	}
}

void boost::do_fetch()
{
	if (prebuilt())
		fetch_prebuilt();
	else
		fetch_from_source();
}

void boost::do_build_and_install()
{
	if (prebuilt())
		build_and_install_prebuilt();
	else
		build_and_install_from_source();
}

void boost::fetch_prebuilt()
{
	cx().trace(context::generic, "using prebuilt boost");

	const auto file = run_tool(downloader(prebuilt_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void boost::build_and_install_prebuilt()
{
	op::copy_file_to_dir_if_better(cx(),
		lib_path(arch::x64) / python_dll(),
		conf().path().install_bin());
}

void boost::fetch_from_source()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void boost::bootstrap()
{
	write_config_jam();

	const auto bootstrap = source_path() / "bootstrap.bat";

	run_tool(process_runner(process()
		.binary(bootstrap)
		.external_error_log(source_path() / "bootstrap.log")
		.cwd(source_path())));
}

void boost::build_and_install_from_source()
{
	if (fs::exists(b2_exe()))
	{
		cx().trace(context::bypass,
			"{} exists, boost already bootstrapped", b2_exe());
	}
	else
	{
		bootstrap();
	}

	do_b2(
		{"thread", "date_time", "filesystem", "locale", "program_options"},
		"static", "static", arch::x64);

	do_b2(
		{"thread", "date_time", "filesystem", "locale"},
		"static", "static", arch::x86);

	do_b2(
		{"thread", "date_time", "locale", "program_options"},
		"static", "shared", arch::x64);

	do_b2(
		{"thread", "date_time", "python", "atomic"},
		"shared", "shared", arch::x64);

	op::copy_file_to_dir_if_better(cx(),
		lib_path(arch::x64) / python_dll(),
		conf().path().install_bin());
}

void boost::do_b2(
	const std::vector<std::string>& components,
	const std::string& link, const std::string& runtime_link, arch a)
{
	run_tool(process_runner(process()
		.binary(b2_exe())
		.arg("address-model=",  address_model_for_arch(a))
		.arg("link=",           link)
		.arg("runtime-link=",   runtime_link)
		.arg("toolset=",        "msvc-" + vs::toolset())
		.arg("--user-config=",  config_jam_file())
		.arg("--stagedir=",     root_lib_path(a))
		.arg("--libdir=",       root_lib_path(a))
		.args(map(components, [](auto&& c) { return "--with-" + c; }))
		.env(env::vs(a))
		.cwd(source_path())));
}

void boost::write_config_jam()
{
	auto forward_slashes = [](auto&& p)
	{
		std::string s = path_to_utf8(p);
		return replace_all(s, "\\", "/");
	};

	std::ostringstream oss;

	oss
		<< "using python\n"
		<< "  : " << python_version_for_jam() << "\n"
		<< "  : " << forward_slashes(python::python_exe()) << "\n"
		<< "  : " << forward_slashes(python::include_path()) << "\n"
		<< "  : " << forward_slashes(python::build_path()) << "\n"
		<< "  : <address-model>64\n"
		<< "  : <define>BOOST_ALL_NO_LIB=1\n"
		<< "  ;";

	cx().trace(context::generic,
		"writing config file at {}:", config_jam_file());

	for_each_line(oss.str(), [&](auto&& line)
	{
		cx().trace(context::generic, "        {}", line);
	});


	op::write_text_file(cx(), encodings::utf8, config_jam_file(), oss.str());
}


boost::version_info boost::parsed_version()
{
	// 1.72.0-b1-rc1
	// everything but 1.72 is optional
	std::regex re(
		"(\\d+)\\."       // 1.
		"(\\d+)"          // 72
		"(?:"
			"\\.(\\d+)"   // .0
			"(?:"
				"-(.+)"   // -b1-rc1
			")?"
		")?");

	std::smatch m;

	const auto s = version();

	if (!std::regex_match(s, m, re))
		gcx().bail_out(context::generic, "bad boost version '{}'", s);

	return {m[1], m[2], m[3], m[4]};
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
	const auto underscores = replace_all(version(), ".", "_");
	return make_prebuilt_url("boost_prebuilt_" + underscores + ".7z");
}

url boost::source_url()
{
	return
		"https://dl.bintray.com/boostorg/release/" +
		boost_version_no_tags() + "/source/" +
		boost_version_all_underscores() + ".zip";
}

fs::path boost::b2_exe()
{
	return source_path() / "b2.exe";
}

std::string boost::python_dll()
{
	std::ostringstream oss;

	// builds something like boost_python38-vc142-mt-x64-1_72.dll

	// boost_python38-
	oss << "boost_python" << python_version_for_dll() + "-";

	// vc142-
	oss << "vc" + replace_all(version_vs(), ".", "") << "-";

	// mt-x64-1_72
	oss << "mt-x64-" << boost_version_no_patch_underscores();

	oss << ".dll";

	return oss.str();
}

std::string boost::python_version_for_dll()
{
	const auto v = python::parsed_version();

	// 38
	return v.major + v.minor;
}

std::string boost::python_version_for_jam()
{
	const auto v = python::parsed_version();

	// 3.8
	return v.major + "." + v.minor;
}

std::string boost::boost_version_no_patch_underscores()
{
	const auto v = parsed_version();

	// 1_72
	return v.major + "_" + v.minor;
}

std::string boost::boost_version_no_tags()
{
	const auto v = parsed_version();

	// 1.72[.1]
	std::string s = v.major + "." + v.minor;

	if (v.patch != "")
		s += "." + v.patch;

	return s;
}

std::string boost::boost_version_no_tags_underscores()
{
	return replace_all(boost_version_no_tags(), ".", "_");
}

std::string boost::boost_version_all_underscores()
{
	const auto v = parsed_version();

	// boost_1_72[_0[_b1_rc1]]
	std::string s = "boost_" + v.major + "_" + v.minor;

	if (v.patch != "")
		s += "_" + v.patch;

	if (v.rest != "")
		s += "_" + replace_all(v.rest, "-", "_");

	return s;
}

std::string boost::address_model_for_arch(arch a)
{
	switch (a)
	{
		case arch::x86:
			return "32";

		case arch::x64:
		case arch::dont_care:
			return "64";

		default:
			gcx().bail_out(context::generic, "boost: bad arch");
	}
}

}	// namespace

#include "pch.h"
#include "tasks.h"
#include "../core/process.h"

namespace mob::tasks
{

sip::sip()
	: basic_task("sip")
{
}

std::string sip::version()
{
	return conf().version().get("sip");
}

std::string sip::version_for_pyqt()
{
	return conf().version().get("pyqt_sip");
}

bool sip::prebuilt()
{
	return false;
}

fs::path sip::source_path()
{
	return conf().path().build() / ("sip-" + version());
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

	const auto s = version_for_pyqt();

	if (!std::regex_match(s, m, re))
		gcx().bail_out(context::generic, "bad pyqt sip version {}", s);

	// 12.7
	const auto dir = m[1].str() + "." + m[2].str();

	return source_path() / "sipbuild" / "module" / "source" / dir;
}

void sip::do_clean(clean c)
{
	if (is_set(c, clean::reextract))
	{
		cx().trace(context::reextract, "deleting {}", source_path());
		op::delete_directory(cx(), source_path(), op::optional);
		return;
	}

	if (is_set(c, clean::rebuild))
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
		if (conf().global().redownload())
		{
			cx().trace(context::redownload, "deleting {}", download_file());
			op::delete_file(cx(), download_file(), op::optional);
		}
		else
		{
			cx().trace(context::bypass,
				"sip: {} already exists", download_file());

			return;
		}
	}

	run_tool(pip(pip::download)
		.package("sip")
		.version(version()));
}

void sip::generate()
{
	const auto header = source_path() / "sip.h";

	if (fs::exists(header))
	{
		if (conf().global().rebuild())
		{
			cx().trace(context::rebuild, "ignoring {}", header);
		}
		else
		{
			cx().trace(context::bypass, "{} already exists", header);
			return;
		}
	}

	run_tool(mob::python()
		.root(source_path())
		.arg("setup.py")
		.arg("install"));


	const std::string filename = "sip-module-script.py";
	const fs::path src = python::scripts_path() / filename;
	const fs::path backup = python::scripts_path() / (filename + ".bak");
	const fs::path dest = python::scripts_path() / (filename + ".acp");

	if (!fs::exists(backup))
	{
		const std::string utf8 = op::read_text_file(cx(), encodings::utf8, src);
		op::write_text_file(cx(), encodings::acp, dest, utf8);
		op::replace_file(cx(), src, dest, backup);
	}

	// generate sip.h, will be copied to python's include directory, used
	// by plugin_python
	run_tool(process_runner(process()
		.binary(sip_module_exe())
		.chcp(850)
		.stdout_encoding(encodings::acp)
		.stderr_encoding(encodings::acp)
		.arg("--sip-h")
		.arg(pyqt::pyqt_sip_module_name())
		.cwd(source_path())));
}

fs::path sip::download_file()
{
	return conf().path().cache() / ("sip-" + version() + ".tar.gz");
}

}	// namespace

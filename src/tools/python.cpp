#include "pch.h"
#include "tools.h"
#include "../core/process.h"
#include "../tasks/tasks.h"

namespace mob
{

python::python()
	: basic_process_runner("python")
{
}

python& python::root(const fs::path& p)
{
	root_ = p;
	return *this;
}

python& python::arg(const std::string& s)
{
	args_.push_back(s);
	return *this;
}

void python::do_run()
{
	// python is a bit finicky for utf8, so:
	//  1) chcp changes the codepage to utf8
	//  2) `-X utf8` and the PYTHONUTF8 environment variable are set, which
	//     is probably redundant

	auto p = process()
		.binary(tasks::python::python_exe())
		.chcp(65001)
		.stdout_encoding(encodings::utf8)
		.stderr_encoding(encodings::utf8)
		.stderr_filter([&](process::filter& f)
		{
			// filter out crap from setuptools
			if (f.line.find("zip_safe flag not set") != std::string::npos)
				f.lv = context::level::trace;
			else if (f.line.find("module references __file__") != std::string::npos)
				f.lv = context::level::trace;
		})
		.arg("-X", "utf8");  // forces utf8

	for (auto&& a : args_)
		p.arg(a);

	p
		.cwd(root_)
		.env(this_env::get().set("PYTHONUTF8", "1"));  // forces utf8

	execute_and_join(p);
}


pip::pip(ops op)
	: basic_process_runner("pip"), op_(op)
{
}

pip& pip::package(const std::string& s)
{
	package_ = s;
	return *this;
}

pip& pip::version(const std::string& s)
{
	version_ = s;
	return *this;
}

pip& pip::file(const fs::path& p)
{
	file_ = p;
	return *this;
}

void pip::do_run()
{
	switch (op_)
	{
		case ensure:
			do_ensure();
			break;

		case install:
			do_install();
			break;

		case download:
			do_download();
			break;

		default:
			cx().bail_out(context::generic, "pip unknown op {}", op_);
	}
}

void pip::do_ensure()
{
	// ensure
	execute_and_join(process()
		.stderr_filter([](auto&& f)
		{
			// this spits out two warnings about not being on PATH and suggests
			// to add --no-warn-script-location, but that's not actually a valid
			// parameter for `ensurepip` and it fails, unlike the `install`
			// commands below
			//
			// so just filter it out

			if (f.line.find("which is not on PATH") != std::string::npos)
				f.lv = context::level::debug;
			else if (f.line.find("Consider adding this") != std::string::npos)
				f.lv = context::level::debug;
		})
		.binary(tasks::python::python_exe())
		.arg("-m", "ensurepip"));


	// upgrade
	execute_and_join(process()
		.binary(tasks::python::python_exe())
		.arg("-m pip")
		.arg("install")
		.arg("--no-warn-script-location")
		.arg("--upgrade pip"));


	// ssl errors while downloading through python without certifi
	execute_and_join(process()
		.binary(tasks::python::python_exe())
		.arg("-m pip")
		.arg("install")
		.arg("--no-warn-script-location")
		.arg("certifi"));
}

void pip::do_install()
{
	auto p = process()
		.binary(tasks::python::python_exe())
		.chcp(65001)
		.stdout_encoding(encodings::utf8)
		.stderr_encoding(encodings::utf8)
		.arg("-X", "utf8")
		.arg("-m", "pip")
		.arg("install")
		.arg("--no-warn-script-location")
		.arg("--disable-pip-version-check");

	if (!package_.empty())
		p.arg(package_ + "==" + version_);
	else if (!file_.empty())
		p.arg(file_);

	p.env(this_env::get().set("PYTHONUTF8", "1"));

	execute_and_join(p);
}

void pip::do_download()
{
	execute_and_join(process()
		.binary(tasks::python::python_exe())
		.chcp(65001)
		.stdout_encoding(encodings::utf8)
		.stderr_encoding(encodings::utf8)
		.arg("-X", "utf8")
		.arg("-m", "pip")
		.arg("download")
		.arg("--no-binary=:all:")
		.arg("--no-deps")
		.arg("-d", conf().path().cache())
		.arg(package_ + "==" + version_)
		.env(this_env::get().set("PYTHONUTF8", "1")));
}

}	// namespace

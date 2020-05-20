#include "pch.h"
#include "tools.h"
#include "../utility.h"
#include "../op.h"
#include "../conf.h"
#include "../tasks/tasks.h"

namespace mob
{

tool::tool(std::string name)
	: cx_(nullptr), name_(std::move(name)), interrupted_(false)
{
}

tool::tool(tool&& t)
	: name_(std::move(t.name_)), interrupted_(t.interrupted_.load())
{
}

tool& tool::operator=(tool&& t)
{
	cx_ = t.cx_;
	name_ = std::move(t.name_);
	interrupted_ = t.interrupted_.load();
	return *this;
}

void tool::set_name(const std::string& s)
{
	name_ = s;
}

const std::string& tool::name() const
{
	return name_;
}

void tool::run(context& cx)
{
	cx_ = &cx;

	cx_->set_tool(this);
	guard g([&]{ cx_->set_tool(nullptr); });

	do_run();
}

void tool::interrupt()
{
	if (!interrupted_)
	{
		cx().debug(context::interruption, "interrupting {}", name_);
		interrupted_ = true;
		do_interrupt();
	}
}

bool tool::interrupted() const
{
	return interrupted_;
}

const context& tool::cx() const
{
	if (cx_)
		return *cx_;
	else
		return gcx();
}


fs::path perl::binary()
{
	return conf::tool_by_name("perl");
}

fs::path nasm::binary()
{
	return conf::tool_by_name("nasm");
}

fs::path qt::installation_path()
{
	return conf::path_by_name("qt_install");
}

fs::path qt::bin_path()
{
	return conf::path_by_name("qt_bin");
}

std::string qt::version()
{
	return conf::version_by_name("qt");
}

std::string qt::vs_version()
{
	return conf::version_by_name("qt_vs");
}


vs::vs(ops o)
	: basic_process_runner("vs"), op_(o)
{
}

fs::path vs::devenv_binary()
{
	return conf::tool_by_name("devenv");
}

fs::path vs::installation_path()
{
	return conf::path_by_name("vs");
}

fs::path vs::vswhere()
{
	return conf::tool_by_name("vswhere");
}

fs::path vs::vcvars()
{
	return conf::tool_by_name("vcvars");
}

std::string vs::version()
{
	return conf::version_by_name("vs");
}

std::string vs::year()
{
	return conf::version_by_name("vs_year");
}

std::string vs::toolset()
{
	return conf::version_by_name("vs_toolset");
}

std::string vs::sdk()
{
	return conf::version_by_name("sdk");
}

vs& vs::solution(const fs::path& sln)
{
	sln_ = sln;
	return *this;
}

void vs::do_run()
{
	switch (op_)
	{
		case upgrade:
		{
			do_upgrade();
			break;
		}

		default:
		{
			cx().bail_out(context::generic, "vs unknown op {}", op_);
		}
	}
}

void vs::do_upgrade()
{
	if (fs::exists(sln_.parent_path() / "UpgradeLog.htm"))
	{
		debug("project already upgraded");
		return;
	}

	process_
		.binary(devenv_binary())
		.env(env::vs(arch::x64))
		.arg("/upgrade")
		.arg(sln_);

	execute_and_join();
}


nuget::nuget(fs::path sln)
	: basic_process_runner("nuget"), sln_(std::move(sln))
{
	process_
		.binary(binary())
		.arg("restore")
		.arg(sln_)
		.cwd(sln_.parent_path());
}

fs::path nuget::binary()
{
	return conf::tool_by_name("nuget");
}

void nuget::do_run()
{
	execute_and_join();
}


pip_install::pip_install()
	: basic_process_runner("pip install")
{
}

pip_install& pip_install::package(const std::string& s)
{
	package_ = s;
	return *this;
}

pip_install& pip_install::version(const std::string& s)
{
	version_ = s;
	return *this;
}

pip_install& pip_install::file(const fs::path& p)
{
	file_ = p;
	return *this;
}

void pip_install::do_run()
{
	process_
		.binary(python::python_exe())
		.chcp(65001)
		.stdout_encoding(encodings::utf8)
		.stderr_encoding(encodings::utf8)
		.arg("-X", "utf8")
		.arg("-m", "pip")
		.arg("install")
		.arg("--no-warn-script-location")
		.arg("--disable-pip-version-check");

	if (!package_.empty())
		process_.arg(package_ + "==" + version_);
	else if (!file_.empty())
		process_.arg(file_);

	process_
		.env(this_env::get()
			.set("PYTHONUTF8", "1"));

	execute_and_join();
}

}	// namespace

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
	name_ = std::move(t.name_);
	interrupted_ = t.interrupted_.load();
	return *this;
}

std::string tool::name() const
{
	std::string s = do_name();
	if (!s.empty())
		return s;

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
		cx_->debug(context::interruption, "interrupting " + name_);
		interrupted_ = true;
		do_interrupt();
	}
}

bool tool::interrupted() const
{
	return interrupted_;
}


devenv_upgrade::devenv_upgrade(fs::path sln)
	: basic_process_runner("upgrade project"), sln_(std::move(sln))
{
}

void devenv_upgrade::do_run()
{
	if (fs::exists(sln_.parent_path() / "UpgradeLog.htm"))
	{
		debug("project already upgraded");
		return;
	}

	process_
		.binary(tools::devenv::binary())
		.env(env::vs(arch::x64))
		.arg("/upgrade")
		.arg(sln_);

	execute_and_join();
}


nuget::nuget(fs::path sln)
	: basic_process_runner("nuget"), sln_(std::move(sln))
{
	process_
		.binary(tools::nuget::binary())
		.arg("restore")
		.arg(sln_)
		.cwd(sln_.parent_path());
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
		.arg("-m", "pip")
		.arg("install")
		.arg("--no-warn-script-location")
		.arg("--disable-pip-version-check");

	if (!package_.empty())
		process_.arg(package_ + "==" + version_);
	else if (!file_.empty())
		process_.arg(file_);

	execute_and_join();
}

}	// namespace

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


transifex::transifex(ops o) :
	basic_process_runner("transifex"), op_(o),
	stdout_(context::level::trace), min_(100), force_(false)
{
}

fs::path transifex::binary()
{
	return conf::tool_by_name("tx");
}

transifex& transifex::root(const fs::path& p)
{
	root_ = p;
	return *this;
}

transifex& transifex::api_key(const std::string& key)
{
	key_ = key;
	return *this;
}

transifex& transifex::url(const mob::url& u)
{
	url_ = u;
	return *this;
}

transifex& transifex::minimum(int percent)
{
	min_ = percent;
	return *this;
}

transifex& transifex::stdout_level(context::level lv)
{
	stdout_ = lv;
	return *this;
}

transifex& transifex::force(bool b)
{
	force_ = b;
	return *this;
}

void transifex::do_run()
{
	switch (op_)
	{
		case init:
			do_init();
			break;

		case config:
			do_config();
			break;

		case pull:
			do_pull();
			break;

		default:
			cx().bail_out(context::generic, "tx unknown op {}", op_);
	}
}

void transifex::do_init()
{
	op::create_directories(cx(), root_, op::unsafe);

	// exit code is 2 when the directory already contains a .tx

	process_ = process()
		.binary(binary())
		.success_exit_codes({0, 2})
		.flags(process::ignore_output_on_success)
		.arg("init")
		.arg("--no-interactive")
		.cwd(root_);

	execute_and_join();
}

void transifex::do_config()
{
	if (url_.empty())
		cx().bail_out(context::generic, "missing transifex url");

	op::create_directories(cx(), root_, op::unsafe);

	process_ = process()
		.binary(binary())
		.stdout_level(stdout_)
		.arg("config")
		.arg("mapping-remote")
		.arg(url_)
		.env(this_env::get()
			.set("TX_TOKEN", key_))
		.cwd(root_);

	execute_and_join();
}

void transifex::do_pull()
{
	op::create_directories(cx(), root_, op::unsafe);

	process_ = process()
		.binary(binary())
		.stdout_level(stdout_)
		.arg("pull")
		.arg("--all")
		.arg("--parallel")
		.arg("--no-interactive")
		.arg("--minimum-perc", min_)
		.env(this_env::get()
			.set("TX_TOKEN", key_))
		.cwd(root_);

	if (force_)
		process_.arg("--force");

	execute_and_join();
}


lrelease::lrelease()
	: basic_process_runner("lrelease")
{
}

fs::path lrelease::binary()
{
	return conf::path_by_name("qt_bin") / conf::tool_by_name("lrelease");
}

lrelease& lrelease::project(const std::string& name)
{
	project_ = name;
	return *this;
}

lrelease& lrelease::add_source(const fs::path& ts_file)
{
	sources_.push_back(ts_file);
	return *this;
}

lrelease& lrelease::out(const fs::path& dir)
{
	out_ = dir;
	return *this;
}

fs::path lrelease::qm_file() const
{
	if (sources_.empty())
		cx().bail_out(context::generic, "lrelease: no sources");

	const auto lang = trim_copy(path_to_utf8(sources_[0].stem()));
	if (lang.empty())
	{
		cx().bail_out(context::generic,
			"lrelease: bad file name '{}'", sources_[0]);
	}

	return project_ + "_" + lang + ".qm";
}

void lrelease::do_run()
{
	const auto qm = qm_file();

	process_ = process()
		.binary(binary())
		.arg("-silent")
		.stderr_filter([](auto&& f)
		{
			if (f.line.find("dropping duplicate") != -1)
				f.lv = context::level::debug;
			else if (f.line.find("try -verbose") != -1)
				f.lv = context::level::debug;
		});


	for (auto&& s : sources_)
		process_.arg(s);

	process_
		.arg("-qm", (out_ / qm));

	execute_and_join();
}

}	// namespace

#include "pch.h"
#include "tools.h"
#include "../utility.h"
#include "../core/op.h"
#include "../core/conf.h"
#include "../core/process.h"
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
	return conf().tool().get("perl");
}

fs::path nasm::binary()
{
	return conf().tool().get("nasm");
}

fs::path qt::installation_path()
{
	return conf().path().qt_install();
}

fs::path qt::bin_path()
{
	return conf().path().get("qt_bin");
}

std::string qt::version()
{
	return conf().version().get("qt");
}

std::string qt::vs_version()
{
	return conf().version().get("qt_vs");
}


vs::vs(ops o)
	: basic_process_runner("vs"), op_(o)
{
}

fs::path vs::devenv_binary()
{
	return conf().tool().get("devenv");
}

fs::path vs::installation_path()
{
	return conf().path().get("vs");
}

fs::path vs::vswhere()
{
	return conf().tool().get("vswhere");
}

fs::path vs::vcvars()
{
	return conf().tool().get("vcvars");
}

std::string vs::version()
{
	return conf().version().get("vs");
}

std::string vs::year()
{
	return conf().version().get("vs_year");
}

std::string vs::toolset()
{
	return conf().version().get("vs_toolset");
}

std::string vs::sdk()
{
	return conf().version().get("sdk");
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
		cx().debug(context::generic, "project already upgraded");
		return;
	}

	set_process(process()
		.binary(devenv_binary())
		.env(env::vs(arch::x64))
		.arg("/upgrade")
		.arg(sln_));

	execute_and_join();
}


fs::path vswhere::find_vs()
{
	auto p = process()
		.binary(vs::vswhere())
		.arg("-products", "*")
		.arg("-prerelease")
		.arg("-version", vs::version())
		.arg("-property", "installationPath")
		.stdout_flags(process::keep_in_string)
		.stderr_flags(process::inherit);

	p.run();
	p.join();

	if (p.exit_code() != 0)
		return {};

	return trim_copy(p.stdout_string());
}


nuget::nuget(fs::path sln)
	: basic_process_runner("nuget"), sln_(std::move(sln))
{
	set_process(process()
		.binary(binary())
		.arg("restore")
		.arg(sln_)
		.cwd(sln_.parent_path()));
}

fs::path nuget::binary()
{
	return conf().tool().get("nuget");
}

void nuget::do_run()
{
	execute_and_join();
}

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
	auto p = process()
		.binary(tasks::python::python_exe())
		.chcp(65001)
		.stdout_encoding(encodings::utf8)
		.stderr_encoding(encodings::utf8)
		.stderr_filter([&](process::filter& f)
		{
			if (f.line.find("zip_safe flag not set") != std::string::npos)
				f.lv = context::level::trace;
			else if (f.line.find("module references __file__") != std::string::npos)
				f.lv = context::level::trace;
		})
		.arg("-X", "utf8");

	for (auto&& a : args_)
		p.arg(a);

	p
		.cwd(root_)
		.env(this_env::get()
			.set("PYTHONUTF8", "1"));

	set_process(p);
	execute_and_join();
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
	//
	// this spits out two warnings about not being on PATH and suggests to add
	// --no-warn-script-location, but that's not actually a valid command
	// line parameter for `ensurepip` and it fails, unlike the `install`
	// commands below
	//
	// so just filter it out

	set_process(process()
		.stderr_filter([](auto&& f)
		{
			if (f.line.find("which is not on PATH") != -1)
				f.lv = context::level::debug;
			else if (f.line.find("Consider adding this directory"))
				f.lv = context::level::debug;
		})
		.binary(tasks::python::python_exe())
		.arg("-m", "ensurepip"));

	execute_and_join();


	// upgrade
	set_process(process()
		.binary(tasks::python::python_exe())
		.arg("-m pip")
		.arg("install")
		.arg("--no-warn-script-location")
		.arg("--upgrade pip"));

	execute_and_join();


	// ssl errors while downloading through python without certifi
	set_process(process()
		.binary(tasks::python::python_exe())
		.arg("-m pip")
		.arg("install")
		.arg("--no-warn-script-location")
		.arg("certifi"));

	execute_and_join();
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

	p
		.env(this_env::get()
			.set("PYTHONUTF8", "1"));

	set_process(p);
	execute_and_join();
}

void pip::do_download()
{
	set_process(process()
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
		.env(this_env::get()
			.set("PYTHONUTF8", "1")));

	execute_and_join();
}


transifex::transifex(ops o) :
	basic_process_runner("transifex"), op_(o),
	stdout_(context::level::trace), min_(100), force_(false)
{
}

fs::path transifex::binary()
{
	return conf().tool().get("tx");
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

	set_process(process()
		.binary(binary())
		.success_exit_codes({0, 2})
		.flags(process::ignore_output_on_success)
		.arg("init")
		.arg("--no-interactive")
		.cwd(root_));

	execute_and_join();
}

void transifex::do_config()
{
	if (url_.empty())
		cx().bail_out(context::generic, "missing transifex url");

	op::create_directories(cx(), root_, op::unsafe);

	set_process(process()
		.binary(binary())
		.stdout_level(stdout_)
		.arg("config")
		.arg("mapping-remote")
		.arg(url_)
		.env(this_env::get()
			.set("TX_TOKEN", key_))
		.cwd(root_));

	execute_and_join();
}

void transifex::do_pull()
{
	op::create_directories(cx(), root_, op::unsafe);

	auto p = process()
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
		p.arg("--force");

	set_process(p);
	execute_and_join();
}


lrelease::lrelease()
	: basic_process_runner("lrelease")
{
}

fs::path lrelease::binary()
{
	return conf().tool().get("lrelease");
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

lrelease& lrelease::sources(const std::vector<fs::path>& v)
{
	sources_ = v;
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

	auto p = process()
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
		p.arg(s);

	p
		.arg("-qm", (out_ / qm));

	set_process(p);
	execute_and_join();
}


iscc::iscc(fs::path iss)
	: basic_process_runner("iscc"), iss_(std::move(iss))
{
}

fs::path iscc::binary()
{
	return conf().tool().get("iscc");
}

iscc& iscc::iss(const fs::path& p)
{
	iss_ = p;
	return *this;
}

void iscc::do_run()
{
	if (iss_.empty())
		cx().bail_out(context::generic, "iscc missing iss file");

	set_process(process()
		.binary(binary())
		.arg(iss_));

	execute_and_join();
}

}	// namespace

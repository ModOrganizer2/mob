#include "pch.h"
#include "tools.h"
#include "utility.h"
#include "op.h"
#include "conf.h"

namespace builder
{

fs::path find_vcvars()
{
	const std::vector<std::string> editions =
	{
		"Preview", "Enterprise", "Professional", "Community"
	};

	for (auto&& edition : editions)
	{
		const auto p =
			paths::program_files_x86() /
			"Microsoft Visual Studio" /
			versions::vs_year() /
			edition / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat";

		if (fs::exists(p))
		{
			debug("found " + p.string());
			return p;
		}
	}

	bail_out("couldn't find visual studio");
}

void vcvars()
{
	info("vcvars");

	const fs::path tmp = paths::temp_file();

	const std::string cmd =
		"\"" + find_vcvars().string() + "\" amd64" + redir_nul() + " && "
		"set > \"" + tmp.string() + "\"";

	op::run(cmd);

	std::stringstream ss(read_text_file(tmp));
	op::delete_file(tmp);

	for (;;)
	{
		std::string line;
		std::getline(ss, line);
		if (!ss)
			break;

		const auto sep = line.find('=');

		if (sep == std::string::npos)
			continue;

		const std::string name = line.substr(0, sep);
		const std::string value = line.substr(sep + 1);

		debug("setting env " + name);
		SetEnvironmentVariableA(name.c_str(), nullptr);
		SetEnvironmentVariableA(name.c_str(), value.c_str());
	}
}

process do_cmake(
	const fs::path& build, const fs::path& prefix,
	const std::string& args, const std::string& generator)
{
	std::string cmd = "cmake -G \"" + generator + "\"";

	if (!prefix.empty())
		cmd += " -DCMAKE_INSTALL_PREFIX=\"" + prefix.string() + "\"";

	if (!args.empty())
		cmd += " " + args;

	cmd += " ..";

	return op::run(cmd, build);
}

process do_nmake(
	const fs::path& dir,
	const std::string& what, const std::string& args)
{
	std::ostringstream oss;

	oss << "nmake";

	if (conf::verbose())
		oss << " /C /S";

	if (!args.empty())
		oss << " " << args;

	if (!what.empty())
		oss << " " << what;

	return op::run(oss.str(), dir);
}


tool::tool(std::string name)
	: name_(std::move(name)), interrupted_(false)
{
}

void tool::run()
{
	do_run();
}

void tool::interrupt()
{
	if (!interrupted_)
	{
		info("interrupting " + name_);
		interrupted_ = true;
		do_interrupt();
	}
}

bool tool::interrupted() const
{
	return interrupted_;
}


process_runner::process_runner(std::string name)
	: tool(std::move(name))
{
}

process_runner::process_runner(std::string name, std::string cmd, fs::path cwd)
	: tool(std::move(name)), cmd_(std::move(cmd)), cwd_(std::move(cwd))
{
}

void process_runner::do_run()
{
	set(op::run(cmd_, cwd_));
}

void process_runner::do_interrupt()
{
	process_.interrupt();
}

void process_runner::set(process p)
{
	process_ = std::move(p);
	process_.join();

	if (process_.exit_code() != 0)
		bail_out("command returned " + std::to_string(p.exit_code()));
}


downloader::downloader(url u)
	: tool("downloader"), dl_(paths::cache(), std::move(u))
{
}

fs::path downloader::result() const
{
	return dl_.file();
}

void downloader::do_run()
{
	dl_.start();
	dl_.join();
}

void downloader::do_interrupt()
{
	dl_.interrupt();
}


decompresser::decompresser(fs::path file, fs::path where) :
	process_runner("decompresser"),
	file_(std::move(file)), where_(std::move(where))
{
}

void decompresser::do_run()
{
	info("decompress " + file_.string() + " into " + where_.string());

	if (fs::exists(interrupt_file()))
	{
		debug("found interrupt file " + interrupt_file().string());
		info("previous decompression was interrupted; resuming");
	}
	else if (fs::exists(where_))
	{
		debug("directory " + where_.string() + " already exists");
		return;
	}

	const auto sevenz = "\"" + find_sevenz().string() + "\"";

	//op::delete_directory(where_);
	op::create_directories(where_);

	process p;

	if (file_.string().ends_with(".tar.gz"))
	{
		p = op::run(
			sevenz + " x -so \"" + file_.string() + "\" | " +
			sevenz + " x -aoa -spe -si -ttar -o\"" + where_.string() + "\" " + redir_nul());
	}
	else
	{
		p = op::run(
			sevenz + " x -aoa -spe -bd -bb0 -o\"" + where_.string() + "\" "
			"\"" + file_.string() + "\" " + redir_nul());
	}

	set(std::move(p));

	if (interrupted())
		op::touch(interrupt_file());
	else
		op::delete_file(interrupt_file());
}

fs::path decompresser::interrupt_file() const
{
	return where_ / "_builder_interrupted";
}


cmake_for_nmake::cmake_for_nmake(fs::path r, std::string a, fs::path p) :
	process_runner("cmake_for_nmake"),
	root_(std::move(r)), args_(std::move(a)), prefix_(std::move(p))
{
}

fs::path cmake_for_nmake::build_path()
{
	return "build";
}

void cmake_for_nmake::do_run()
{
	const auto build = root_ / build_path();
	const std::string g = "NMake Makefiles";
	set(do_cmake(build, prefix_, args_, g));
}


cmake_for_vs::cmake_for_vs(fs::path r, std::string a, fs::path p) :
	process_runner("cmake_for_vs"),
	root_(std::move(r)), args_(std::move(a)), prefix_(std::move(p))
{
}

fs::path cmake_for_vs::build_path()
{
	return "vsbuild";
}

void cmake_for_vs::do_run()
{
	const auto build = root_ / build_path();
	const std::string g = "Visual Studio " + versions::vs() + " " + versions::vs_year();
	set(do_cmake(build, prefix_, args_, g));
}


nmake::nmake(fs::path dir, std::string args)
	: process_runner("nmake"), dir_(std::move(dir)), args_(std::move(args))
{
}

void nmake::do_run()
{
	set(do_nmake(dir_, "", args_));
}

nmake_install::nmake_install(fs::path dir, std::string args) :
	process_runner("nmake_install"),
	dir_(std::move(dir)), args_(std::move(args))
{
}

void nmake_install::do_run()
{
	set(do_nmake(dir_, "install", args_));
}

}	// namespace

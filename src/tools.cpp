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
	debug("vcvars");

	const fs::path tmp = paths::temp_file();

	const std::string cmd =
		"\"" + find_vcvars().string() + "\" amd64 && "
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

		//debug("setting env " + name);
		SetEnvironmentVariableA(name.c_str(), nullptr);
		SetEnvironmentVariableA(name.c_str(), value.c_str());
	}
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
	execute_and_join(op::run(cmd_, cwd_));
}

void process_runner::do_interrupt()
{
	process_.interrupt();
}

int process_runner::execute_and_join(process p, bool check_exit_code)
{
	process_ = std::move(p);
	join(check_exit_code);
	return process_.exit_code();
}

int process_runner::execute_and_join(const cmd& c, bool check_exit_code)
{
	return execute_and_join(op::run(c.string(), c.cwd()), check_exit_code);
}

void process_runner::join(bool check_exit_code)
{
	process_.join();

	if (check_exit_code && !interrupted())
	{
		if (process_.exit_code() != 0)
		{
			bail_out(
				process_.cmd() + " returned " +
				std::to_string(process_.exit_code()));
		}
	}
}

int process_runner::exit_code() const
{
	return process_.exit_code();
}


downloader::downloader(url u)
	: tool("downloader")
{
	urls_.push_back(std::move(u));
}

downloader::downloader(std::vector<url> urls)
	: tool("downloader"), urls_(std::move(urls))
{
}

fs::path downloader::file() const
{
	return file_;
}

void downloader::do_run()
{
	// check if one the urls has already been downloaded
	for (auto&& u : urls_)
	{
		const auto file = path_for_url(u);
		if (fs::exists(file))
		{
			file_ = file;
			debug("download " + file_.string() + " already exists");
			return;
		}
	}

	// try them in order
	for (auto&& u : urls_)
	{
		const fs::path file = path_for_url(u);

		dl_.start(u, file);
		dl_.join();

		if (dl_.ok())
		{
			file_ = file;
			return;
		}
	}

	// all failed
	bail_out("all urls failed");
}

void downloader::do_interrupt()
{
	dl_.interrupt();
}

fs::path downloader::path_for_url(const url& u) const
{
	std::string filename;

	std::string url_string = u.string();

	if (url_string.find("sourceforge.net") != std::string::npos)
	{
		// sf downloads end with /download, strip it to get the filename
		const std::string strip = "/download";

		if (url_string.ends_with(strip))
			url_string = url_string.substr(0, url_string.size() - strip.size());

		filename = url(url_string).filename();
	}
	else
	{
		filename = u.filename();
	}

	return paths::cache() / filename;
}


git_clone::git_clone(std::string a, std::string r, std::string b, fs::path w) :
	process_runner("git_clone"),
	author_(std::move(a)),
	repo_(std::move(r)),
	branch_(std::move(b)),
	where_(std::move(w))
{
}

void git_clone::do_run()
{
	const fs::path dot_git = where_ / ".git";

	if (!fs::exists(dot_git))
		clone();
	else
		pull();
}

void git_clone::clone()
{
	execute_and_join(cmd(third_party::git())
		.arg("clone")
		.arg("--recurse-submodules")
		.arg("--depth", "1")
		.arg("--branch", branch_)
		.arg("--quiet", cmd::quiet)
		.arg("-c", "advice.detachedHead=false", cmd::quiet)
		.arg(repo_url())
		.arg(where_));
}

void git_clone::pull()
{
	execute_and_join(cmd(third_party::git())
		.arg("pull")
		.arg("--recurse-submodules")
		.arg("--quiet", cmd::quiet)
		.arg(repo_url())
		.arg(branch_)
		.cwd(where_));
}

url git_clone::repo_url() const
{
	return "https://github.com/" + author_ + "/" + repo_ + ".git";
}


decompresser::decompresser(fs::path file, fs::path where) :
	process_runner("decompresser"),
	file_(std::move(file)), where_(std::move(where))
{
}

void decompresser::do_run()
{
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

	info("decompress " + file_.string() + " into " + where_.string());

	op::touch(interrupt_file());
	op::create_directories(where_);
	directory_deleter delete_output(where_);

	// the -spe from 7z is supposed to figure out if there's a folder in the
	// archive with the same name as the target and extract its content to
	// avoid duplicating the folder
	//
	// however, it fails miserably if there are files along with that folder,
	// which is the case for openssl:
	//
	//  openssl-1.1.1d.tar/
	//   +- openssl-1.1.1d/
	//   +- pax_global_header
	//
	// that pax_global_header makes 7z fail with "unspecified error"
	//
	// so the handling of a duplicate directory is done manually in
	// check_duplicate_directory() below

	std::string c;

	if (file_.string().ends_with(".tar.gz"))
	{
		c = cmd(third_party::sevenz())
				.arg("x")
				.arg("-so", file_)
				.string();

		c += " | ";

		c += cmd(third_party::sevenz())
			.arg("x")
			.arg("-aoa")
			.arg("-si")
			.arg("-ttar")
			.arg("-o", where_, cmd::nospace)
			.string();
	}
	else
	{
		c = cmd(third_party::sevenz())
			.arg("x")
			.arg("-aoa")
			.arg("-bd")
			.arg("-bb0")
			.arg("-o", where_, cmd::nospace)
			.arg(file_)
			.string();
	}

	execute_and_join(op::run(c));
	check_duplicate_directory();

	delete_output.cancel();

	if (!interrupted())
		op::delete_file(interrupt_file());
}

fs::path decompresser::interrupt_file() const
{
	return where_ / "_mob_interrupted";
}

void decompresser::check_duplicate_directory()
{
	const auto dir_name = where_.filename().string();

	// check for a folder with the same name
	if (!fs::exists(where_ / dir_name))
		return;

	// the archive contained a directory with the same name as the output
	// directory

	// delete anything other than this directory; some archives have
	// useless files along with it
	for (auto e : fs::directory_iterator(where_))
	{
		// but don't delete the directory itself
		if (e.path().filename() == dir_name)
			continue;

		// or the interrupt file
		if (e.path().filename() == interrupt_file().filename())
			continue;

		if (!fs::is_regular_file(e.path()))
		{
			// don't know what to do with archives that have the
			// same directory _and_ other directories
			bail_out(
				"check_duplicate_directory: " + e.path().string() + " is "
				"yet another directory");
		}

		op::delete_file(e.path());
	}

	// now there should only be two things in this directory: another
	// directory with the same name and the interrupt file

	// give it a temp name in case there's yet another directory with the
	// same name in it
	const auto temp_dir_name = where_ / ("_mob_" + dir_name );
	op::rename(where_ / dir_name, where_ / temp_dir_name);

	// move the content of the directory up
	for (auto e : fs::directory_iterator(where_ / temp_dir_name))
		op::move_to_directory(e.path(), where_);

	// delete the old directory, which should be empty now
	op::delete_directory(where_ / temp_dir_name);
}


patcher::patcher(fs::path patch_dir, fs::path output_dir) :
	process_runner("patcher"),
	patches_(std::move(patch_dir)), output_(std::move(output_dir))
{
}

void patcher::do_run()
{
	if (!fs::exists(patches_))
		return;

	const auto base = cmd(third_party::patch())
		.arg("--read-only", "ignore")
		.arg("--strip", "0")
		.arg("--directory", output_)
		.arg("--quiet", cmd::quiet);

	for (auto e : fs::directory_iterator(patches_))
	{
		if (!e.is_regular_file())
			continue;

		const auto p = e.path();
		if (p.extension() != ".patch")
		{
			warn(
				"file without .patch extension " + p.string() + " "
				"in patches directory " + patches_.string());

			continue;
		}

		const auto check = cmd(base)
			.arg("--dry-run")
			.arg("--force")
			.arg("--reverse")
			.arg("--input", p);

		const auto apply = cmd(base)
			.arg("--forward")
			.arg("--batch")
			.arg("--input", p);

		{
			// check
			if (execute_and_join(check, false) == 0)
			{
				debug("patch " + p.string() + " already applied");
				continue;
			}
		}

		{
			// apply
			debug("applying patch " + p.string());
			execute_and_join(apply);
		}
	}
}


process do_cmake(
	const fs::path& build, const fs::path& prefix,
	const std::string& args, const std::string& generator)
{
	auto c = cmd(third_party::cmake())
		.arg("-G", generator)
		.arg("-DCMAKE_INSTALL_MESSAGE=NEVER")
		.arg("--log-level", "WARNING", cmd::quiet);

	if (!prefix.empty())
		c.arg("-DCMAKE_INSTALL_PREFIX=", prefix, cmd::nospace);

	c
		.arg(args)
		.arg("..");

	return op::run(c.string(), build);
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
	execute_and_join(do_cmake(build, prefix_, args_, g));
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
	execute_and_join(do_cmake(build, prefix_, args_, g));
}


jom::jom(fs::path dir, std::string target, std::string args, flags f) :
	process_runner("jom " + target),
	dir_(std::move(dir)), target_(std::move(target)),
	args_(std::move(args)), flags_(f)
{
}

void jom::do_run()
{
	auto c = cmd(third_party::jom())
		.arg("/C", cmd::quiet)
		.arg("/S", cmd::quiet)
		.arg("/K");

	if (flags_ & single_job)
		c.arg("/J", "1");

	c
		.arg(args_)
		.arg(target_)
		.cwd(dir_);

	const bool check_exit_code = !(flags_ & accept_failure);
	execute_and_join(c, check_exit_code);
}


msbuild::msbuild(
	fs::path sln,
	std::vector<std::string> projects,
	std::vector<std::string> params) :
		process_runner("msbuild"),
		sln_(std::move(sln)),
		projects_(std::move(projects)),
		params_(std::move(params))
{
}

void msbuild::do_run()
{
	auto c = cmd(third_party::msbuild())
		.arg("-nologo")
		.arg("-maxCpuCount")
		.arg("-property:UseMultiToolTask=true")
		.arg("-property:EnforceProcessCountAcrossBuilds=true")
		.arg("-property:Configuration=Release")
		.arg("-property:Platform=x64")
		.arg("-property:PlatformToolset=" + versions::vs_toolset())
		.arg("-property:WindowsTargetPlatformVersion=" + versions::sdk())
		.arg("-verbosity:minimal", cmd::quiet)
		.arg("-consoleLoggerParameters:ErrorsOnly", cmd::quiet);

	if (!projects_.empty())
		c.arg("-target:" + builder::join(projects_, ","));

	for (auto&& p : params_)
		c.arg("-property:" + p);

	c
		.arg(sln_)
		.cwd(sln_.parent_path());

	execute_and_join(c);
}

}	// namespace

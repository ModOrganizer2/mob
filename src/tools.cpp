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

		//debug("setting env " + name);
		SetEnvironmentVariableA(name.c_str(), nullptr);
		SetEnvironmentVariableA(name.c_str(), value.c_str());
	}
}


process do_cmake(
	const fs::path& build, const fs::path& prefix,
	const std::string& args, const std::string& generator)
{
	std::string cmd = "\"" + third_party::cmake().string() + "\"";

	cmd += " -G \"" + generator + "\" -DCMAKE_INSTALL_MESSAGE=NEVER";

	if (!prefix.empty())
		cmd += " -DCMAKE_INSTALL_PREFIX=\"" + prefix.string() + "\"";

	if (!conf::verbose())
		cmd += " --log-level=WARNING";

	if (!args.empty())
		cmd += " " + args;

	cmd += " ..";

	return op::run(cmd, build);
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
		const auto file = curl_downloader::path_for_url(paths::cache(), u);
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
		dl_.start(paths::cache(), std::move(u));
		dl_.join();

		if (dl_.ok())
		{
			file_ = dl_.file();
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
	std::ostringstream oss;

	oss
		<< "\"" + third_party::git().string() + "\""
		<< " clone"
		<< " --recurse-submodules"
		<< " --depth 1"
		<< " --branch \"" + branch_ + "\" "
		<< " \"" + repo_url().string() + "\" "
		<< " \"" + where_.string() + "\"";

	execute_and_join(op::run(oss.str()));
}

void git_clone::pull()
{
	std::ostringstream oss;

	oss
		<< "\"" + third_party::git().string() + "\""
		<< " pull"
		<< " --recurse-submodules"
		<< " \"" + repo_url().string() + "\" "
		<< " \"" << branch_ << "\"";

	execute_and_join(op::run(oss.str(), where_));
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

	const auto sevenz = "\"" + third_party::sevenz().string() + "\"";

	op::create_directories(where_);
	directory_deleter delete_output(where_);

	process p;

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

	if (file_.string().ends_with(".tar.gz"))
	{
		p = op::run(
			sevenz + " x -so \"" + file_.string() + "\" | " +
			sevenz + " x -aoa -si -ttar -o\"" + where_.string() + "\" " + redir_nul());
	}
	else
	{
		p = op::run(
			sevenz + " x -aoa -bd -bb0 -o\"" + where_.string() + "\" "
			"\"" + file_.string() + "\" " + redir_nul());
	}

	execute_and_join(std::move(p));
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

	std::ostringstream oss;

	oss
		<< "\"" << third_party::patch().string() << "\" "
		<< "--read-only=ignore "
		<< "--strip=0 "
		<< "--directory=\"" << output_.string() << "\" ";

	if (!conf::verbose())
		oss << "--quiet ";

	const std::string base = oss.str();

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

		const std::string input = "--input=\"" + p.string() + "\"";
		const std::string check = base + " --dry-run --force --reverse " + input;
		const std::string apply = base + " --forward --batch " + input;

		{
			// check
			if (execute_and_join(op::run(check), false) == 0)
			{
				debug("patch " + p.string() + " already applied");
				continue;
			}
		}

		{
			// apply
			info("applying patch " + p.string());
			execute_and_join(op::run(apply));
		}
	}
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
	std::ostringstream oss;
	oss << "\"" << third_party::jom().string() << "\"";

	if (!conf::verbose())
		oss << " /C /S";

	oss << " /K ";

	if (flags_ & single_job)
		oss << " /J 1";

	if (!args_.empty())
		oss << " " << args_;

	if (!target_.empty())
		oss << " " << target_;

	oss << redir_nul();

	const bool check_exit_code = !(flags_ & accept_failure);
	execute_and_join(op::run(oss.str(), dir_), check_exit_code);
}

}	// namespace

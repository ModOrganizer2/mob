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


basic_process_runner::basic_process_runner(std::string name)
	: tool(std::move(name))
{
}

void basic_process_runner::do_interrupt()
{
	process_.interrupt();
}

int basic_process_runner::execute_and_join(process p, bool check_exit_code)
{
	process_ = std::move(p);
	join(check_exit_code);
	return process_.exit_code();
}

int basic_process_runner::execute_and_join(const cmd& c, bool check_exit_code)
{
	return execute_and_join(op::run(c.string(), c.cwd()), check_exit_code);
}

void basic_process_runner::join(bool check_exit_code)
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

int basic_process_runner::exit_code() const
{
	return process_.exit_code();
}


downloader::downloader()
	: tool("downloader")
{
}

downloader::downloader(builder::url u)
	: tool("downloader")
{
	urls_.push_back(std::move(u));
}

downloader& downloader::url(const builder::url& u)
{
	urls_.push_back(u);
	return *this;
}

fs::path downloader::result() const
{
	return file_;
}

void downloader::do_run()
{
	dl_.reset(new curl_downloader);

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

		dl_->start(u, file);
		dl_->join();

		if (dl_->ok())
		{
			file_ = file;
			return;
		}
	}

	if (interrupted())
		return;

	// all failed
	bail_out("all urls failed");
}

void downloader::do_interrupt()
{
	if (dl_)
		dl_->interrupt();
}

fs::path downloader::path_for_url(const builder::url& u) const
{
	std::string filename;

	std::string url_string = u.string();

	if (url_string.find("sourceforge.net") != std::string::npos)
	{
		// sf downloads end with /download, strip it to get the filename
		const std::string strip = "/download";

		if (url_string.ends_with(strip))
			url_string = url_string.substr(0, url_string.size() - strip.size());

		filename = builder::url(url_string).filename();
	}
	else
	{
		filename = u.filename();
	}

	return paths::cache() / filename;
}


git_clone::git_clone()
	: basic_process_runner("git_clone")
{
}

git_clone& git_clone::org(const std::string& name)
{
	org_ = name;
	return *this;
}

git_clone& git_clone::repo(const std::string& name)
{
	repo_ = name;
	return *this;
}

git_clone& git_clone::branch(const std::string& name)
{
	branch_ = name;
	return *this;
}

git_clone& git_clone::output(const fs::path& dir)
{
	where_ = dir;
	return *this;
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
	execute_and_join(cmd(third_party::git(), cmd::stdout_is_verbose)
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
	execute_and_join(cmd(third_party::git(), cmd::stdout_is_verbose)
		.arg("pull")
		.arg("--recurse-submodules")
		.arg("--quiet", cmd::quiet)
		.arg(repo_url())
		.arg(branch_)
		.cwd(where_));
}

url git_clone::repo_url() const
{
	return "https://github.com/" + org_ + "/" + repo_ + ".git";
}


decompresser::decompresser()
	: basic_process_runner("decompresser")
{
}

decompresser& decompresser::file(const fs::path& file)
{
	file_ = file;
	return *this;
}

decompresser& decompresser::output(const fs::path& dir)
{
	where_ = dir;
	return *this;
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
		c = cmd(third_party::sevenz(), cmd::noflags)
				.arg("x")
				.arg("-so", file_)
				.string();

		c += " | ";

		c += cmd(third_party::sevenz(), cmd::noflags)
			.arg("x")
			.arg("-aoa")
			.arg("-si")
			.arg("-ttar")
			.arg("-o", where_, cmd::nospace)
			.string();
	}
	else
	{
		c = cmd(third_party::sevenz(), cmd::stdout_is_verbose)
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


patcher::patcher()
	: basic_process_runner("patcher")
{
}

patcher& patcher::task(const std::string& name)
{
	patches_ = paths::patches() / name;
	return *this;
}

patcher& patcher::root(const fs::path& dir)
{
	output_ = dir;
	return *this;
}

void patcher::do_run()
{
	if (!fs::exists(patches_))
		return;

	const auto base = cmd(third_party::patch(), cmd::noflags)
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


cmake::cmake() :
	basic_process_runner("cmake"),
	gen_(nmake), cmd_(third_party::cmake(), cmd::stdout_is_verbose)
{
}

cmake& cmake::generator(generators g)
{
	gen_ = g;
	return *this;
}

cmake& cmake::root(const fs::path& p)
{
	root_ = p;
	return *this;
}

cmake& cmake::prefix(const fs::path& s)
{
	prefix_ = s;
	return *this;
}

cmake& cmake::def(const std::string& s)
{
	cmd_.arg("-D" + s);
	return *this;
}

fs::path cmake::result() const
{
	return output_;
}

void cmake::do_run()
{
	std::string g;

	switch (gen_)
	{
		case nmake:
		{
			output_ = root_ / "build";
			g = "NMake Makefiles";
			break;
		}

		case vs:
		{
			output_ = root_ / "vsbuild";
			g = "Visual Studio " + versions::vs() + " " + versions::vs_year();
			break;
		}
	}


	cmd_
		.arg("-G", "\"" + g + "\"")
		.arg("-DCMAKE_INSTALL_MESSAGE=NEVER", cmd::quiet)
		.arg("--log-level", "WARNING", cmd::quiet);

	if (!prefix_.empty())
		cmd_.arg("-DCMAKE_INSTALL_PREFIX=", prefix_, cmd::nospace);

	cmd_.arg("..");
	cmd_.cwd(output_);

	execute_and_join(cmd_);
}



jom::jom() :
	basic_process_runner("jom"),
	cmd_(third_party::jom(), cmd::stdout_is_verbose), flags_(noflags)
{
}

jom& jom::path(const fs::path& p)
{
	cmd_.cwd(p);
	return *this;
}

jom& jom::target(const std::string& s)
{
	target_ = s;
	return *this;
}

jom& jom::def(const std::string& s)
{
	cmd_.arg(s);
	return *this;
}

jom& jom::flag(flags f)
{
	flags_ = f;
	return *this;
}

int jom::result() const
{
	return exit_code();
}

void jom::do_run()
{
	cmd_
		.arg("/C", cmd::quiet)
		.arg("/S", cmd::quiet)
		.arg("/K");

	if (flags_ & single_job)
		cmd_.arg("/J", "1");

	cmd_.arg(target_);

	const bool check_exit_code = !(flags_ & accept_failure);
	execute_and_join(cmd_, check_exit_code);
}


msbuild::msbuild()
	: basic_process_runner("msbuild")
{
}

msbuild& msbuild::solution(const fs::path& sln)
{
	sln_ = sln;
	return *this;
}

msbuild& msbuild::projects(const std::vector<std::string>& names)
{
	projects_ = names;
	return *this;
}

msbuild& msbuild::parameters(const std::vector<std::string>& params)
{
	params_ = params;
	return *this;
}

void msbuild::do_run()
{
	// 14.2 to v142
	const auto toolset = "v" + replace_all(versions::vs_toolset(), ".", "");

	auto c = cmd(third_party::msbuild(), cmd::noflags)
		.arg("-nologo")
		.arg("-maxCpuCount")
		.arg("-property:UseMultiToolTask=true")
		.arg("-property:EnforceProcessCountAcrossBuilds=true")
		.arg("-property:Configuration=Release")
		.arg("-property:Platform=x64")
		.arg("-property:PlatformToolset=" + toolset)
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

	execute_and_join(cmd(third_party::devenv(), cmd::stdout_is_verbose)
		.arg("/upgrade")
		.arg(sln_));
}

}	// namespace

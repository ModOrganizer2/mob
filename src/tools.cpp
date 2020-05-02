#include "pch.h"
#include "tools.h"
#include "utility.h"
#include "op.h"
#include "conf.h"
#include "tasks/tasks.h"

namespace mob
{

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

int basic_process_runner::execute_and_join()
{
	process_.run();
	join();
	return process_.exit_code();
}

void basic_process_runner::join()
{
	process_.join();
}

int basic_process_runner::exit_code() const
{
	return process_.exit_code();
}


process_runner::process_runner(process p)
{
	process_ = std::move(p);
}

int process_runner::result() const
{
	return exit_code();
}

void process_runner::do_run()
{
	execute_and_join();
//	execute_and_join(process(
//		arch_,
//		"\"" + bin_.string() + "\" " + cmd_.string(),
//		cwd_,
//		env_,
//		flags_));
}


downloader::downloader()
	: tool("downloader")
{
}

downloader::downloader(mob::url u)
	: tool("downloader")
{
	urls_.push_back(std::move(u));
}

downloader& downloader::url(const mob::url& u)
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

fs::path downloader::path_for_url(const mob::url& u) const
{
	std::string filename;

	std::string url_string = u.string();

	if (url_string.find("sourceforge.net") != std::string::npos)
	{
		// sf downloads end with /download, strip it to get the filename
		const std::string strip = "/download";

		if (url_string.ends_with(strip))
			url_string = url_string.substr(0, url_string.size() - strip.size());

		filename = mob::url(url_string).filename();
	}
	else
	{
		filename = u.filename();
	}

	return paths::cache() / filename;
}


url make_github_url(const std::string& org, const std::string& repo)
{
	return "https://github.com/" + org + "/" + repo + ".git";
}


git_clone::git_clone()
	: basic_process_runner("git_clone")
{
}

git_clone& git_clone::url(const mob::url& u)
{
	url_ = u;
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
	if (url_.empty() || where_.empty())
		bail_out("git_clone missing parameters");

	const fs::path dot_git = where_ / ".git";

	if (!fs::exists(dot_git))
		clone();
	else
		pull();
}

void git_clone::clone()
{
	process_ = process()
		.binary(third_party::git())
		.flags(process::stdout_is_verbose)
		.arg("clone")
		.arg("--recurse-submodules")
		.arg("--depth", "1")
		.arg("--branch", branch_)
		.arg("--quiet", process::quiet)
		.arg("-c", "advice.detachedHead=false", process::quiet)
		.arg(url_)
		.arg(where_);

	execute_and_join();
}

void git_clone::pull()
{
	process_ = process()
		.binary(third_party::git())
		.flags(process::stdout_is_verbose)
		.arg("pull")
		.arg("--recurse-submodules")
		.arg("--quiet", process::quiet)
		.arg(url_)
		.arg(branch_)
		.cwd(where_);

	execute_and_join();
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

	if (file_.string().ends_with(".tar.gz"))
	{
		auto extract_tar = process()
			.binary(third_party::sevenz())
			.arg("x")
			.arg("-so", file_);

		auto extract_gz = process()
			.binary(third_party::sevenz())
			.arg("x")
			.arg("-aoa")
			.arg("-si")
			.arg("-ttar")
			.arg("-o", where_, process::nospace);

		process_ = process::pipe(extract_tar, extract_gz);
	}
	else
	{
		process_ = process()
			.binary(third_party::sevenz())
			.flags(process::stdout_is_verbose)
			.arg("x")
			.arg("-aoa")
			.arg("-bd")
			.arg("-bb0")
			.arg("-o", where_, process::nospace)
			.arg(file_);
	}

	execute_and_join();
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

patcher& patcher::file(const fs::path& p)
{
	file_ = p;
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

	if (file_.empty())
	{
		for (auto e : fs::directory_iterator(patches_))
		{
			if (!e.is_regular_file())
				continue;

			const auto p = e.path();

			if (p.extension() == ".manual_patch")
			{
				// skip manual patches
				continue;
			}
			else if (p.extension() != ".patch")
			{
				warn(
					"file without .patch extension " + p.string() + " "
					"in patches directory " + patches_.string());

				continue;
			}

			do_patch(p);
		}
	}
	else
	{
		do_patch(patches_ / file_);
	}
}

void patcher::do_patch(const fs::path& patch_file)
{
	const auto base = process()
		.binary(third_party::patch())
		.arg("--read-only", "ignore")
		.arg("--strip", "0")
		.arg("--directory", output_)
		.arg("--quiet", process::quiet);

	const auto check = process(base)
		.flags(process::allow_failure)
		.arg("--dry-run")
		.arg("--force")
		.arg("--reverse")
		.arg("--input", patch_file);

	const auto apply = process(base)
		.arg("--forward")
		.arg("--batch")
		.arg("--input", patch_file);

	{
		// check
		process_ = check;
		if (execute_and_join() == 0)
		{
			debug("patch " + patch_file.string() + " already applied");
			return;
		}
	}

	{
		// apply
		process_ = apply;
		debug("applying patch " + patch_file.string());
		execute_and_join();
	}
}


cmake::cmake()
	: basic_process_runner("cmake"), gen_(jom), arch_(arch::def)
{
	process_
		.binary(third_party::cmake())
		.flags(process::stdout_is_verbose);
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
	process_.arg("-D" + s);
	return *this;
}

cmake& cmake::architecture(arch a)
{
	arch_ = a;
	return *this;
}

fs::path cmake::result() const
{
	return output_;
}

void cmake::do_run()
{
	if (conf::clean())
	{
		for (auto&& [k, g] : all_generators())
		{
			op::delete_directory(root_ / g.output_dir(arch::x86));
			op::delete_directory(root_ / g.output_dir(arch::x64));
		}
	}

	const auto& g = get_generator();
	output_ = root_ / (g.output_dir(arch_));

	process_
		.arg("-G", "\"" + g.name + "\"")
		.arg("-DCMAKE_BUILD_TYPE=Release")
		.arg("-DCMAKE_INSTALL_MESSAGE=NEVER", process::quiet)
		.arg("--log-level", "WARNING", process::quiet)
		.arg(g.get_arch(arch_));

	if (!prefix_.empty())
		process_.arg("-DCMAKE_INSTALL_PREFIX=", prefix_, process::nospace);

	process_
		.arg("..")
		.env(env::vs(arch_))
		.cwd(output_);

	execute_and_join();
}

const std::map<cmake::generators, cmake::gen_info>&
cmake::all_generators() const
{
	static const std::map<generators, gen_info> map =
	{
		{ generators::jom, {"build", "NMake Makefiles JOM", "", "" }},

		{ generators::vs, {
			"vsbuild",
			"Visual Studio " + versions::vs() + " " + versions::vs_year(),
			 "Win32",
			"x64"
		}}
	};

	return map;
}

const cmake::gen_info& cmake::get_generator() const
{
	const auto& map = all_generators();

	auto itor = map.find(gen_);
	if (itor == map.end())
		bail_out("unknown generator");

	return itor->second;
}

std::string cmake::gen_info::get_arch(arch a) const
{
	switch (a)
	{
		case arch::x86:
		{
			if (x86.empty())
				return {};
			else
				return "-A " + x86;
		}

		case arch::x64:
		{
			if (x64.empty())
				return {};
			else
				return "-A" + x64;
		}

		case arch::dont_care:
			return {};

		default:
			bail_out("gen_info::get_arch(): bad arch");
	}
}

std::string cmake::gen_info::output_dir(arch a) const
{
	switch (a)
	{
		case arch::x86:
			return (dir + "_32");

		case arch::x64:
		case arch::dont_care:
			return dir;

		default:
			bail_out("gen_info::get_arch(): bad arch");
	}
}



jom::jom()
	: basic_process_runner("jom"), arch_(arch::def)
{
	process_
		.binary(third_party::jom());
}

jom& jom::path(const fs::path& p)
{
	process_.cwd(p);
	return *this;
}

jom& jom::target(const std::string& s)
{
	target_ = s;
	return *this;
}

jom& jom::def(const std::string& s)
{
	process_.arg(s);
	return *this;
}

jom& jom::flag(flags f)
{
	flags_ = f;
	return *this;
}

jom& jom::architecture(arch a)
{
	arch_ = a;
	return *this;
}

int jom::result() const
{
	return exit_code();
}

void jom::do_run()
{
	process_
		.arg("/C", process::quiet)
		.arg("/S", process::quiet)
		.arg("/K");

	if (flags_ & single_job)
		process_.arg("/J", "1");

	process_.arg(target_);

	if (flags_ & allow_failure)
	{
		process_.flags(process::flags_t(
			process_.flags() | process::allow_failure));
	}

	process_.env(env::vs(arch_));

	execute_and_join();
}


msbuild::msbuild()
	: basic_process_runner("msbuild"), config_("Release"), arch_(arch::def)
{
	process_
		.binary(third_party::msbuild());
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

msbuild& msbuild::config(const std::string& s)
{
	config_ = s;
	return *this;
}

msbuild& msbuild::platform(const std::string& s)
{
	platform_ = s;
	return *this;
}

msbuild& msbuild::architecture(arch a)
{
	arch_ = a;
	return *this;
}

void msbuild::do_run()
{
	// 14.2 to v142
	const auto toolset = "v" + replace_all(versions::vs_toolset(), ".", "");

	std::string plat;

	if (platform_.empty())
	{
		switch (arch_)
		{
			case arch::x86:
				plat = "Win32";
				break;

			case arch::x64:
				plat = "x64";
				break;

			case arch::dont_care:
			default:
				bail_out("msbuild::do_run(): bad arch");
		}
	}
	else
	{
		plat = platform_;
	}


	process_
		.arg("-nologo")
		.arg("-maxCpuCount")
		.arg("-property:UseMultiToolTask=true")
		.arg("-property:EnforceProcessCountAcrossBuilds=true")
		.arg("-property:Configuration=", config_, process::quote)
		.arg("-property:PlatformToolset=" + toolset)
		.arg("-property:WindowsTargetPlatformVersion=" + versions::sdk())
		.arg("-property:Platform=", plat, process::quote)
		.arg("-verbosity:minimal", process::quiet)
		.arg("-consoleLoggerParameters:ErrorsOnly", process::quiet);

	if (!projects_.empty())
		process_.arg("-target:" + mob::join(projects_, ","));

	for (auto&& p : params_)
		process_.arg("-property:" + p);

	process_
		.arg(sln_)
		.cwd(sln_.parent_path())
		.env(env::vs(arch_));

	execute_and_join();
}


devenv_upgrade::devenv_upgrade(fs::path sln)
	: basic_process_runner("upgrade project"), sln_(std::move(sln))
{
	process_
		.binary(third_party::devenv())
		.flags(process::stdout_is_verbose)
		.env(env::vs(arch::x64));
}

void devenv_upgrade::do_run()
{
	if (fs::exists(sln_.parent_path() / "UpgradeLog.htm"))
	{
		debug("project already upgraded");
		return;
	}

	process_
		.arg("/upgrade")
		.arg(sln_);

	execute_and_join();
}


nuget::nuget(fs::path sln)
	: basic_process_runner("nuget"), sln_(std::move(sln))
{
	process_
		.binary(third_party::nuget())
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

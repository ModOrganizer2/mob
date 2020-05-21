#include "pch.h"
#include "commands.h"
#include "utility.h"
#include "conf.h"
#include "net.h"
#include "tasks/tasks.h"
#include "tools/tools.h"

namespace mob
{

std::string version()
{
	return "mob 3.0";
}


void help(const clipp::group& g, const std::string& more)
{
#pragma warning(suppress: 4548)
	auto usage_df = clipp::doc_formatting()
		.first_column(4)
		.doc_column(30);

	auto options_df = clipp::doc_formatting()
		.first_column(4)
		.doc_column(30);

	u8cout
		<< "Usage:\n" << clipp::usage_lines(g, "mob", usage_df)
		<< "\n\n"
		<< "Options:\n"
		<< clipp::documentation(g, options_df)
		<< "\n";

	if (!more.empty())
		u8cout << "\n" << more << "\n";
}



command::common_options command::common;

command::command(flags f)
	: picked_(false), help_(false), flags_(f), code_(0)
{
}

clipp::group command::common_options_group()
{
	auto& o = common;
	const auto master = master_ini_filename();

	return
		(clipp::repeatable(clipp::option("-i", "--ini")
			& clipp::value("FILE") >> o.inis))
			% "path to the ini file",

		(clipp::option("--dry") >> o.dry)
			%  "simulates filesystem operations",

		(clipp::option("-l", "--log-level")
			&  clipp::value("LEVEL") >> o.output_log_level)
			%  "0 is silent, 6 is max",

		(clipp::option("--file-log-level")
			&  clipp::value("LEVEL") >> o.file_log_level)
			%  "overrides --log-level for the log file",

		(clipp::option("--log-file")
			&  clipp::value("FILE") >> o.log_file)
			%  "path to log file",

		(clipp::option("-d", "--destination")
			& clipp::value("DIR") >> o.prefix)
			% ("base output directory, will contain build/, install/, etc."),

		(clipp::repeatable(clipp::option("-s", "--set")
			& clipp::value("OPTION", o.options)))
			%  "sets an option, such as 'versions/openssl=1.2'",

		(clipp::option("--no-default-inis") >> o.no_default_inis)
			% "disables auto loading of ini files, only uses --ini; the first"
			  "--ini must be the master ini file";
}

void command::force_exit_code(int code)
{
	code_ = code;
}

void command::force_pick()
{
	picked_ = true;
}

void command::force_help()
{
	help_ = true;
}

bool command::picked() const
{
	return picked_;
}

bool command::wants_help() const
{
	return help_;
}

clipp::group command::group()
{
	return do_group();
}

void command::convert_cl_to_conf()
{
	auto& o = common;

	if (o.file_log_level == -1)
		o.file_log_level = o.output_log_level;

	if (o.output_log_level >= 0)
	{
		o.options.push_back(
			"global/output_log_level=" +
			std::to_string(o.output_log_level));
	}

	if (o.file_log_level > 0)
	{
		o.options.push_back(
			"global/file_log_level=" +
			std::to_string(o.file_log_level));
	}

	if (!o.log_file.empty())
		o.options.push_back("global/log_file=" + o.log_file);

	if (o.dry)
		o.options.push_back("global/dry=true");

	if (!o.prefix.empty())
		o.options.push_back("paths/prefix=" + o.prefix);
}

int command::gather_inis(bool verbose)
{
	auto& o = common;

	if (o.no_default_inis && o.inis.empty())
	{
		u8cerr
			<< "--no-default-inis requires at least one --ini for the "
			<< "master ini file\n";

		return 1;
	}

	try
	{
		inis_ = find_inis(!o.no_default_inis, o.inis, verbose);
		return 0;
	}
	catch(bailed&)
	{
		return 1;
	}
}

int command::prepare_options(bool verbose)
{
	convert_cl_to_conf();
	return gather_inis(verbose);
}

int command::run()
{
	if (help_)
	{
		help(group(), do_doc());
		return 0;
	}

	if (flags_ & requires_options)
	{
		const int r = prepare_options(false);
		if (r != 0)
			return r;

		init_options(inis_, common.options);
		log_options();

		if (!verify_options())
			return 1;
	}

	const auto r = do_run();

	if (code_)
		return *code_;

	return r;
}

const std::vector<fs::path>& command::inis() const
{
	return inis_;
}


clipp::group version_command::do_group()
{
	return clipp::group(
		clipp::command("version", "-v", "--version").set(picked_));
}

int version_command::do_run()
{
	u8cout << version() << "\n";
	return 0;
}


clipp::group help_command::do_group()
{
	return clipp::group(
		clipp::command("-h", "--help").set(picked_));
}

int help_command::do_run()
{
#pragma warning(suppress: 4548)
	auto doc = (command::common_options_group(), (clipp::value("command")));

	const auto master = master_ini_filename();

	help(doc,
		"Commands:\n"
		"    help       shows this message\n"
		"    version    shows the version\n"
		"    list       lists available tasks\n"
		"    options    lists all options and their values from the inis\n"
		"    build      builds tasks\n"
		"    release    creates a release or a devbuild\n"
		"    git        manages the git repos\n"
		"    cmake      runs cmake in a directory\n"
		"    inis       lists the INIs used by mob (debug)\n"
		"\n\n"
		"Invoking `mob -d some/prefix build` builds everything. Do \n"
		"`mob build <task name>...` to build specific tasks. See\n"
		"`mob command --help` for more information about a command.\n"
		"\n"
		"INI files\n"
		"\n"
		"By default, mob will look for a master INI `" + master + "` in the \n"
		"root directory (typically where mob.exe resides). Once mob has\n"
		"found the master INI, it will look for the same filename in the\n"
		"current directory, if different from the root. If found, both will\n"
		"be loaded, but the one in the current directory will override the\n"
		"the other. Additional INIs can be specified with --ini, those will\n"
		"be loaded after the two mentioned above. Use --no-default-inis to\n"
		"only disable auto detection and only use --ini.");

	return 0;
}


options_command::options_command()
	: command(requires_options)
{
}

clipp::group options_command::do_group()
{
	return clipp::group(
		clipp::command("options").set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% ("shows this message")
	);
}

int options_command::do_run()
{
	dump_available_options();
	return 0;
}

std::string options_command::do_doc()
{
	return "Lists the final value of all options found by loading the INIs.";
}


build_command::build_command()
	: command(requires_options)
{
}

clipp::group build_command::do_group()
{
	return
		(clipp::command("build")).set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% ("shows this message"),

		(clipp::option("-g", "--redownload") >> redownload_)
			% "redownloads archives, see --reextract",

		(clipp::option("-e", "--reextract") >> reextract_)
			% "deletes source directories and re-extracts archives",

		(clipp::option("-b", "--rebuild") >> rebuild_)
			%  "cleans and rebuilds projects",

		(clipp::option("-n", "--new") >> clean_)
			% "deletes everything and starts from scratch",

		(clipp::option("-p", "--no-pull") >> nopull_)
			% "clones repos if necessary, but never pulls once cloned",

		(clipp::option("--revert-ts") >> revert_ts_)
			% "reverts all the .ts files in a repo before pulling to avoid "
			  "merge errors",

		(clipp::option("--keep-msbuild") >> keep_msbuild_)
			% "don't terminate msbuild.exe instances after building",

		(clipp::opt_values(
			clipp::match::prefix_not("-"), "task", tasks_))
			% "tasks to run; specify 'super' to only build modorganizer "
			"projects";
}

void build_command::convert_cl_to_conf()
{
	command::convert_cl_to_conf();

	if (redownload_ || clean_)
		common.options.push_back("global/redownload=true");

	if (reextract_ || clean_)
		common.options.push_back("global/reextract=true");

	if (rebuild_ || clean_)
		common.options.push_back("global/rebuild=true");

	if (nopull_)
		common.options.push_back("options/no_pull=true");

	if (revert_ts_)
		common.options.push_back("options/revert_ts=true");
}

int build_command::do_run()
{
	try
	{
		curl_init curl;

		if (!tasks_.empty())
			run_tasks(tasks_);
		else
			run_all_tasks();

		{
			using namespace std::chrono;

			std::ofstream out("timings.txt");

			auto write = [&](auto&& inst)
			{
				for (auto&& t : inst.instrumented_tasks())
				{
					for (auto&& tp : t.tps)
					{
						out
							<< inst.instrumentable_name() << "\t"
							<< (duration_cast<milliseconds>(tp.start).count() / 1000.0) << "\t"
							<< (duration_cast<milliseconds>(tp.end).count() / 1000.0) << "\t"
							<< t.name << "\n";
					}
				}
			};

			for (auto&& tk : get_all_tasks())
				write(*tk);

			write(git_submodule_adder::instance());
		}

		if (!keep_msbuild_)
			terminate_msbuild();

		mob::gcx().info(mob::context::generic, "mob done");
		return 0;
	}
	catch(bailed&)
	{
		error("bailing out");
		return 1;
	}
}

void build_command::terminate_msbuild()
{
	if (conf::dry())
		return;

	system("taskkill /im msbuild.exe /f > NUL 2>&1");
}


clipp::group list_command::do_group()
{
	return clipp::group(
		clipp::command("list").set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% ("shows this message")
	);
}

int list_command::do_run()
{
	list_tasks();
	return 0;
}

std::string list_command::do_doc()
{
	return
		"Lists all available tasks. The special task name `super` is a\n"
		"shorthand for all modorganizer tasks.";
}


release_command::release_command()
	: command(requires_options)
{
}

void release_command::make_bin()
{
	const auto out = out_ / make_filename("");
	u8cout << "making binary archive " << path_to_utf8(out) << "\n";

	op::archive_from_glob(gcx(),
		paths::install_bin() / "*", out, {"__pycache__"});
}

void release_command::make_pdbs()
{
	const auto out = out_ / make_filename("pdbs");
	u8cout << "making pdbs archive " << path_to_utf8(out) << "\n";

	op::archive_from_glob(gcx(),
		paths::install_pdbs() / "*", out, {"__pycache__"});
}

void release_command::make_src()
{
	const auto out = out_ / make_filename("src");
	u8cout << "making src archive " << path_to_utf8(out) << "\n";

	const std::vector<std::string> ignore =
	{
		"\\..+",     // dot files
		".*\\.log",
		".*\\.tlog",
		".*\\.dll",
		".*\\.exe",
		".*\\.lib",
		".*\\.obj",
		".*\\.ts",
		".*\\.aps",
		"vsbuild"
	};

	const std::vector<std::regex> ignore_re(ignore.begin(), ignore.end());

	std::vector<fs::path> files;
	std::size_t total_size = 0;

	if (!fs::exists(modorganizer::super_path()))
	{
		gcx().bail_out(context::generic,
			"modorganizer super path not found: {}",
			modorganizer::super_path());
	}

	walk_dir(modorganizer::super_path(), files, ignore_re, total_size);

	// should be below 20MB
	const std::size_t max_expected_size = 20 * 1024 * 1024;
	if (total_size >= max_expected_size)
	{
		gcx().warning(context::generic,
			"total size of source files would be {}, expected something "
			"below {}, something might be wrong",
			total_size, max_expected_size);

		if (!force_)
		{
			gcx().bail_out(context::generic,
				"bailing out, use --force to ignore");
		}
	}

	op::archive_from_files(gcx(),
		files, modorganizer::super_path(), out);
}

void release_command::walk_dir(
	const fs::path& dir, std::vector<fs::path>& files,
	const std::vector<std::regex>& ignore_re, std::size_t& total_size)
{
	for (auto e : fs::directory_iterator(dir))
	{
		const auto p = e.path();
		const auto filename = path_to_utf8(p.filename());

		bool ignored = false;

		for (auto&& re : ignore_re)
		{
			if (std::regex_match(filename, re))
			{
				ignored = true;
				break;
			}
		}

		if (ignored)
			continue;

		if (e.is_directory())
		{
			walk_dir(e.path(), files, ignore_re, total_size);
		}
		else if (e.is_regular_file())
		{
			total_size += fs::file_size(p);
			files.push_back(p);
		}
	}
}

fs::path release_command::make_filename(const std::string& what) const
{
	std::string filename = "Mod.Organizer";

	if (!version_.empty())
		filename += "-" + version_;

	if (!suffix_.empty())
		filename += "-" + suffix_;

	if (!what.empty())
		filename += "-" + what;

	filename += ".7z";

	return filename;
}

clipp::group release_command::do_group()
{
	return clipp::group(
		clipp::command("release").set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% ("shows this message"),

		(
			clipp::option("--no-bin").set(bin_, false) |
			clipp::option("--bin").set(bin_, true)
		) % "sets whether the binary archive is created [default: yes]",

		(
			clipp::option("--no-pdbs").set(pdbs_, false) |
			clipp::option("--pdbs").set(pdbs_, true)
		) % "sets whether the PDBs archive is created [default: yes]",

		(
			clipp::option("--no-src").set(src_, false) |
			clipp::option("--src").set(src_, true)
		) % "sets whether the source archive is created [default: yes]",

		clipp::option("--version-from-exe").set(version_exe_)
			% "retrieves version information from ModOrganizer.exe "
			  "[default]",

		clipp::option("--version-from-rc").set(version_rc_)
			% "retrieves version information from modorganizer/src/version.rc",

		(clipp::option("--rc")
			& clipp::value("PATH") >> utf8_rc_path_)
			% "overrides the path to version.rc",

		(clipp::option("--version")
			& clipp::value("VERSION") >> version_)
			% "overrides the version string",

		(clipp::option("--output-dir")
			& clipp::value("PATH") >> utf8out_)
			% "sets the output directory to use instead of `$prefix/releases`",

		(clipp::option("--suffix")
			& clipp::value("SUFFIX") >> suffix_)
			% "optional suffix to add to the archive filenames",

		clipp::option("--force").set(force_)
			% "ignores file size warnings which could indicate bad paths or "
			  "unexpected files being pulled into the archives"
	);
}

int release_command::do_run()
{
	out_ = fs::path(utf8_to_utf16(utf8out_));
	if (out_.empty())
		out_ = paths::prefix() / "releases";
	else if (out_.is_relative())
		out_ = paths::prefix() / out_;

	rc_path_ = fs::path(utf8_to_utf16(utf8_rc_path_));
	if (rc_path_.empty())
	{
		rc_path_ =
			modorganizer::super_path() / "modorganizer" / "src" / "version.rc";
	}

	if (version_.empty())
	{
		if (version_rc_)
			version_ = version_from_rc();
		else
			version_ = version_from_exe();
	}

	u8cout <<
		">> don't forget to update the version number before making a "
		"release\n";

	if (bin_)
		make_bin();

	if (pdbs_)
		make_pdbs();

	if (src_)
		make_src();

	return 0;
}

std::string release_command::do_doc()
{
	return
		"Creates three archives in the current directory: one from \n"
		"`install/bin/*`, one from `install/pdbs/*` and another with the \n"
		"sources of projects from modorganizer_super.\n"
		"\n"
		"The archive filename is `Mod.Organizer-version-suffix-what.7z`,\n"
		"where:\n"
		"  - `version` is taken from `ModOrganizer.exe`, `version.rc`\n"
		"    or from --version;\n"
		"  - `suffix` is the optional `--suffix` argument;\n"
		"  - `what` is either nothing, `src` or `pdbs`.";
}

std::string release_command::version_from_exe() const
{
	const auto exe = paths::install_bin() / "ModOrganizer.exe";

	// getting version info size
	DWORD dummy = 0;
	const DWORD size = GetFileVersionInfoSizeW(exe.native().c_str(), &dummy);

	if (size == 0)
	{
		const auto e = GetLastError();
		gcx().bail_out(context::generic,
			"can't get file version info size from {}, {}",
			exe, error_message(e));
	}

	// getting version info
	auto buffer = std::make_unique<std::byte[]>(size);

	if (!GetFileVersionInfoW(exe.native().c_str(), 0, size, buffer.get()))
	{
		const auto e = GetLastError();
		gcx().bail_out(context::generic,
			"can't get file version info from {}, {}",
			exe, error_message(e));
	}

	struct LANGANDCODEPAGE
	{
		WORD wLanguage;
		WORD wCodePage;
	};

	void* value_pointer = nullptr;
	unsigned int value_size = 0;

	// getting list of available languages
	auto ret = VerQueryValueW(
		buffer.get(), L"\\VarFileInfo\\Translation",
		&value_pointer, &value_size);

	if (!ret || !value_pointer || value_size == 0)
	{
		const auto e = GetLastError();
		gcx().bail_out(context::generic,
			"VerQueryValueW() for translations failed on {}, {}",
			exe, error_message(e));
	}

	// number of languages
	const auto count = value_size / sizeof(LANGANDCODEPAGE);
	if (count == 0)
		gcx().bail_out(context::generic, "no languages found in {}", exe);

	// using the first language in the list to get FileVersion
	const auto* lcp = static_cast<LANGANDCODEPAGE*>(value_pointer);

	const auto sub_block = ::fmt::format(
		L"\\StringFileInfo\\{:04x}{:04x}\\FileVersion",
		lcp->wLanguage, lcp->wCodePage);

	ret = VerQueryValueW(
		buffer.get(), sub_block.c_str(), &value_pointer, &value_size);

	if (!ret || !value_pointer || value_size == 0)
	{
		gcx().bail_out(context::generic,
			"language {} not found in {}", sub_block, exe);
	}

	// value_size includes the null terminator
	return utf16_to_utf8(std::wstring(
		static_cast<wchar_t*>(value_pointer),
		value_size - 1));
}

std::string release_command::version_from_rc() const
{
	// matching: #define VER_FILEVERSION_STR "2.2.1\0"
	std::regex re(R"(#define VER_FILEVERSION_STR "(.+)\\0")");

	const std::string rc = op::read_text_file(
		gcx(), encodings::utf8, rc_path_);

	std::smatch m;
	std::string v;

	for_each_line(rc, [&](std::string_view line)
	{
		std::string line_s(line);
		if (std::regex_match(line_s, m, re))
			v = m[1];
	});

	if (v.empty())
	{
		gcx().bail_out(context::generic,
			"can't find version string in {}", rc_path_);
	}

	return v;
}


git_command::git_command()
	: command(requires_options)
{
}

clipp::group git_command::do_group()
{
	return clipp::group(
		clipp::command("git").set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% ("shows this message"),

		"set-remotes" %
		(clipp::command("set-remotes").set(mode_, modes::set_remotes),
			(clipp::required("-u", "--username")
				& clipp::value("USERNAME") >> username_)
				% "git username",

			(clipp::required("-e", "--email")
				& clipp::value("EMAIL") >> email_)
				% "git email",

			(clipp::option("-k", "--key")
				& clipp::value("PATH") >> key_)
				% "path to putty key",

			(clipp::option("-s", "--no-push").set(nopush_)
				% "disables pushing to 'upstream' by changing the push url "
				  "to 'nopushurl' to avoid accidental pushes"),

			(clipp::option("-p", "--push-origin").set(push_default_)
				% "sets the new 'origin' remote as the default push target")
		)

		|

		"add-remote" %
		(clipp::command("add-remote").set(mode_, modes::add_remote),
			(clipp::required("-n", "--name")
				& clipp::value("NAME") >> remote_)
				% "name of new remote",

			(clipp::required("-u", "--username")
				& clipp::value("USERNAME") >> username_)
				% "git username",

			(clipp::option("-k", "--key")
				& clipp::value("PATH") >> key_)
				% "path to putty key",

			(clipp::option("-p", "--push-origin").set(push_default_)
				% "sets this new remote as the default push target")
		)

		|

		"ignore-ts" %
		(clipp::command("ignore-ts").set(mode_, modes::ignore_ts),
			(
				clipp::command("on").set(tson_, true) |
				clipp::command("off").set(tson_, false)
			)
		)
	);
}

int git_command::do_run()
{
	switch (mode_)
	{
		case modes::set_remotes:
		{
			do_set_remotes();
			break;
		}

		case modes::add_remote:
		{
			do_add_remote();
			break;
		}

		case modes::ignore_ts:
		{
			do_ignore_ts();
			break;
		}

		case modes::none:
		default:
			u8cerr << "bad git mode " << static_cast<int>(mode_) << "\n";
			throw bailed();
	}

	return 0;
}

std::string git_command::do_doc()
{
	return
		"All the commands will go through all modorganizer repos, plus usvfs\n"
		"and NCC.\n"
		"\n"
		"set-remotes\n"
		"For each repo, this first sets the username and email. Then, it\n"
		"will rename the remote 'origin' to 'upstream' and create a new\n"
		"remote 'origin' with the given information. If the remote\n"
		"'upstream' already exists in a repo, nothing happens.\n"
		"\n"
		"add-remote\n"
		"For each repo, adds a new remote with the given information. If a\n"
		"remote with the same name already exists, nothing happens.\n"
		"\n"
		"ignore-ts\n"
		"Toggles the --assume-changed status of all .ts files in all repos.";
}

void git_command::do_set_remotes()
{
	const auto repos = get_repos();

	for (auto&& r : repos)
	{
		u8cout << "setting up " << path_to_utf8(r.filename()) << "\n";
		git::set_credentials(r, username_, email_);
		git::set_remote(r, username_, key_, nopush_, push_default_);
	}
}

void git_command::do_add_remote()
{
	const auto repos = get_repos();

	u8cout
		<< "adding remote '" << remote_ << "' "
		<< "from '" << username_ << "' to repos\n";

	for (auto&& r : repos)
	{
		u8cout << path_to_utf8(r.filename()) << "\n";
		git::add_remote(r, remote_, username_, key_, push_default_);
	}
}

void git_command::do_ignore_ts()
{
	const auto repos = get_repos();

	if (tson_)
		u8cout << "ignoring .ts files\n";
	else
		u8cout << "un-ignoring .ts files\n";

	for (auto&& r : repos)
	{
		u8cout << path_to_utf8(r.filename()) << "\n";
		git::ignore_ts(r, tson_);
	}
}

std::vector<fs::path> git_command::get_repos() const
{
	std::vector<fs::path> v;

	if (fs::exists(usvfs::source_path()))
		v.push_back(usvfs::source_path());

	if (fs::exists(ncc::source_path()))
		v.push_back(ncc::source_path());


	const auto super = modorganizer::super_path();

	if (fs::exists(super))
	{
		for (auto e : fs::directory_iterator(super))
		{
			if (!e.is_directory())
				continue;

			const auto p = e.path();
			if (path_to_utf8(p.filename()).starts_with("."))
				continue;

			v.push_back(p);
		}
	}

	return v;
}


cmake_command::cmake_command()
	: command(requires_options)
{
}

clipp::group cmake_command::do_group()
{
	return clipp::group(
		clipp::command("cmake").set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% "shows this message",

		(clipp::option("-G", "--generator")
			& clipp::value("GEN") >> gen_)
			% ("sets the -G option for cmake [default: VS]"),

		(clipp::option("-c", "--cmd")
			& clipp::value("CMD") >> cmd_)
			% "overrides the cmake command line [default: \"..\"]",

		(
			clipp::option("--x64").set(x64_, true) |
			clipp::option("--x86").set(x64_, false)
		)
			% "whether to use the x64 or x86 vcvars; if -G is not set, "
			  "whether to pass \"-A Win32\" or \"-A x64\" for the default "
			  "VS generator [default: x64]",

		(clipp::option("--install-prefix")
			& clipp::value("PATH") >> prefix_)
			% "sets CMAKE_INSTALL_PREFIX [default: empty]",

		(clipp::value("PATH") >> path_)
			% "path from which to run `cmake`"
	);
}

int cmake_command::do_run()
{
	auto t = modorganizer::create_cmake_tool(fs::path(utf8_to_utf16(path_)));

	t.generator(gen_);
	t.cmd(cmd_);
	t.prefix(prefix_);
	t.output(path_);

	if (!x64_)
		t.architecture(arch::x86);

	context cxcopy(gcx());
	t.run(cxcopy);

	return 0;
}

std::string cmake_command::do_doc()
{
	return
		"Runs `cmake ..` in the given directory with the same command line\n"
		"as the one used for modorganizer projects.";
}


clipp::group inis_command::do_group()
{
	return clipp::group(
		clipp::command("inis").set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% ("shows this message")
	);
}

int inis_command::do_run()
{
	const auto r = prepare_options(true);
	if (r != 0)
		return r;

	u8cout << "\nhigher number overrides lower\n";

	const auto v = inis();
	for (std::size_t i=0; i<v.size(); ++i)
		u8cout << (i + 1) << ") " << path_to_utf8(v[i]) << "\n";

	return 0;
}

std::string inis_command::do_doc()
{
	return "Shows which INIs are found.";
}

}	// namespace

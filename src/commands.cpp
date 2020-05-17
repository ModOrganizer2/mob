#include "pch.h"
#include "commands.h"
#include "utility.h"
#include "conf.h"
#include "net.h"
#include "tasks/tasks.h"

namespace mob
{

std::string version()
{
	return "mob 2.0";
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

	return
		(clipp::repeatable(clipp::option("-i", "--ini")
			& clipp::value("FILE") >> o.inis))
			% ("path to the ini file"),

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
			% "disables auto detection of ini files, only uses --ini";
}

void command::force_exit_code(int code)
{
	code_ = code;
}

void command::force_pick()
{
	picked_ = true;
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
	return (do_group(),
		(clipp::option("-h", "--help") >> help_)
			% ("shows this message"));
}

int command::run()
{
	if (help_)
	{
		help(group(), do_doc());
		return 0;
	}

	auto& o = common;

	if (o.file_log_level == -1)
		o.file_log_level = o.output_log_level;

	if (o.output_log_level >= 0)
	{
		o.options.push_back(
			"options/output_log_level=" +
			std::to_string(o.output_log_level));
	}

	if (o.file_log_level > 0)
	{
		o.options.push_back(
			"options/file_log_level=" +
			std::to_string(o.file_log_level));
	}

	if (!o.log_file.empty())
		o.options.push_back("options/log_file=" + o.log_file);

	if (o.dry)
		o.options.push_back("options/dry=true");

	if (!o.prefix.empty())
		o.options.push_back("paths/prefix=" + o.prefix);

	do_pre_run();

	if (flags_ & requires_options)
	{
		if (o.no_default_inis && o.inis.empty())
		{
			u8cerr
				<< "--no-default-inis requires at least one --ini for the "
				<< "master ini file\n";

			return 1;
		}

		std::vector<fs::path> inis;
		for (auto&& s : o.inis)
			inis.push_back(s);

		init_options(inis, !o.no_default_inis, o.options);
		log_options();

		if (!verify_options())
			return 1;
	}

	const auto r = do_run();

	if (code_)
		return *code_;

	return r;
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

	const auto mobini = master_ini_filename();

	help(doc,
		"Commands:\n"
		"    help       shows this message\n"
		"    version    shows the version\n"
		"    list       lists available tasks\n"
		"    options    lists available options and default values\n"
		"    build      builds tasks\n"
		"    release    creates a release or a devbuild"
		"\n\n"
		"Invoking `mob -d some/prefix build` builds everything. Do \n"
		"`mob build <task name>...` to build specific tasks. See\n"
		"`mob command --help` for more information about a command.\n"
		"\n"
		"INI files\n"
		"\n"
		"By default, mob will look for a master INI `" + mobini + "` in the \n"
		"current directory and up to three of its parents (so it can also be\n"
		"found from the build directory). Once mob found the master INI, it\n"
		"will look for any other .ini file in the same directory.\n"
		"\n"
		"These additional INI files will be loaded after the master, in\n"
		"lexicographical order, overriding anything the previous INI file\n"
		"may have set.\n"
		"\n"
		"Additional INI files may be specified with --ini. They will be\n"
		"loaded in order after the ones that were auto detected. If the same\n"
		"INI file is found in the directory and on the command line, its\n"
		"position in the load order will be moved to that of the command\n"
		"line.");

	return 0;
}


clipp::group options_command::do_group()
{
	return clipp::group(
		clipp::command("options").set(picked_));
}

int options_command::do_run()
{
	dump_available_options();
	return 0;
}

std::string options_command::do_doc()
{
	return
		"Lists all available options in the form of `section/key = default`.\n"
		"They can be changed in the INI file by setting `key = value` within\n"
		"`[section]` or with `-s section/key=value`.";
}


build_command::build_command()
	: command(requires_options)
{
}

clipp::group build_command::do_group()
{
	return
		(clipp::command("build")).set(picked_),

		(clipp::option("-g", "--redownload") >> redownload_)
			% "redownloads archives, see --reextract",

		(clipp::option("-e", "--reextract") >> reextract_)
			% "deletes source directories and re-extracts archives",

		(clipp::option("-b", "--rebuild") >> rebuild_)
			%  "cleans and rebuilds projects",

		(clipp::option("-n", "--new") >> clean_)
			% "deletes everything and starts from scratch",

		(clipp::option("--keep-msbuild") >> keep_msbuild_)
			% "don't terminate msbuild.exe instances after building",

		(clipp::opt_values(
			clipp::match::prefix_not("-"), "task", tasks_))
			% "tasks to run; specify 'super' to only build modorganizer "
			"projects";
}

void build_command::do_pre_run()
{
	if (redownload_ || clean_)
		common.options.push_back("options/redownload=true");

	if (reextract_ || clean_)
		common.options.push_back("options/reextract=true");

	if (rebuild_ || clean_)
		common.options.push_back("options/rebuild=true");
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

	system("taskkill /im msbuild.exe /f > NUL");
}


clipp::group list_command::do_group()
{
	return clipp::group(
		clipp::command("list").set(picked_));
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

}	// namespace

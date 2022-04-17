#include "pch.h"
#include "commands.h"
#include "../utility.h"
#include "../core/conf.h"
#include "../core/context.h"
#include "../core/op.h"
#include "../core/ini.h"
#include "../tasks/tasks.h"
#include "../tasks/task_manager.h"
#include "../utility/threading.h"

namespace mob
{

void set_sigint_handler();

release_command::release_command()
	: command(requires_options)
{
}

command::meta_t release_command::meta() const
{
	return
	{
		"release",
		"creates a release"
	};
}

void release_command::make_bin()
{
	const auto out = out_ / make_filename("");
	u8cout << "making binary archive " << path_to_utf8(out) << "\n";

	op::archive_from_glob(gcx(),
		conf().path().install_bin() / "*", out, {"__pycache__"});
}

void release_command::make_pdbs()
{
	const auto out = out_ / make_filename("pdbs");
	u8cout << "making pdbs archive " << path_to_utf8(out) << "\n";

	op::archive_from_glob(gcx(),
		conf().path().install_pdbs() / "*", out, {"__pycache__"});
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

	if (!fs::exists(tasks::modorganizer::super_path()))
	{
		gcx().bail_out(context::generic,
			"modorganizer super path not found: {}",
			tasks::modorganizer::super_path());
	}

	// build list list
	walk_dir(tasks::modorganizer::super_path(), files, ignore_re, total_size);

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
		files, tasks::modorganizer::super_path(), out);
}

void release_command::make_uibase()
{
	const auto out = out_ / make_filename("uibase");
	u8cout << "making uibase archive " << path_to_utf8(out) << "\n";

	std::vector<fs::path> files;

	if (!fs::exists(tasks::modorganizer::super_path()))
	{
		gcx().bail_out(context::generic,
			"modorganizer super path not found: {}",
			tasks::modorganizer::super_path());
	}

	op::archive_from_glob(gcx(),
		tasks::modorganizer::super_path() / "uibase" / "src" / "*.h", out, {});
	op::archive_from_glob(gcx(),
		tasks::modorganizer::super_path() / "game_features" / "src" / "*.h", out, {});
	op::archive_from_files(gcx(), { conf().path().install_libs() / "uibase.lib" }, conf().path().install_libs(), out);
}

void release_command::make_installer()
{
	const auto file = "Mod.Organizer-" + version_ + ".exe";
	const auto src = conf().path().install_installer() / file;
	const auto dest = out_;

	u8cout << "copying installer " << file << "\n";

	op::copy_file_to_dir_if_better(gcx(), src, dest);
}

void release_command::walk_dir(
	const fs::path& dir, std::vector<fs::path>& files,
	const std::vector<std::regex>& ignore_re, std::size_t& total_size)
{
	// adds all files that are not in the ignore list to `files`, recursive

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

		"devbuild" %
		(clipp::command("devbuild").set(mode_, modes::devbuild),
			(
				clipp::option("--bin").set(bin_, true) |
				clipp::option("--no-bin").set(bin_, false)
			) % "sets whether the binary archive is created [default: yes]",

			(
				clipp::option("--pdbs").set(pdbs_, true) |
				clipp::option("--no-pdbs").set(pdbs_, false)
			) % "sets whether the PDBs archive is created [default: yes]",

			(
				clipp::option("--src").set(src_, true) |
				clipp::option("--no-src").set(src_, false)
			) % "sets whether the source archive is created [default: yes]",

			//(
			//	clipp::option("--uibase").set(uibase_, true) |
			//	clipp::option("--no-uibase").set(uibase_, false)
			//) % "sets whether the uibase archive is created [default: yes]",

			(
				clipp::option("--inst").set(installer_, true) |
				clipp::option("--no-inst").set(installer_, false)
			) % "sets whether the installer is copied [default: no]",

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
				% "ignores file size warnings and existing release directories"
		)

		|

		"official" %
		(clipp::command("official").set(mode_, modes::official),
			(clipp::value("branch") >> branch_)
				% "use this branch in the super repos"
		)
	);
}

void release_command::convert_cl_to_conf()
{
	command::convert_cl_to_conf();

	if (mode_ == modes::official)
	{
		// force enable translations, installer and tx

		common.options.push_back("task/mo_branch=" + branch_);
		common.options.push_back("translations:task/enabled=true");
		common.options.push_back("installer:task/enabled=true");

		common.options.push_back("transifex/force=true");
		common.options.push_back("transifex/configure=true");
		common.options.push_back("transifex/pull=true");
	}
}

int release_command::do_run()
{
	switch (mode_)
	{
		case modes::devbuild:
			return do_devbuild();

		case modes::official:
			return do_official();

		case modes::none:
		default:
			u8cerr << "bad release mode " << static_cast<int>(mode_) << "\n";
			throw bailed();
	}
}

int release_command::do_devbuild()
{
	prepare();

	u8cout
		<< ">> don't forget to update the version number before making a release\n"
		<< "\n"
		<< "creating release for " << version_ << "\n";

	if (bin_)
		make_bin();

	if (pdbs_)
		make_pdbs();

	if (src_)
		make_src();

	if (uibase_)
		make_uibase();

	if (installer_)
		make_installer();

	return 0;
}

int release_command::do_official()
{
	set_sigint_handler();

	// make sure the given branch exists in all repos, this avoids failure
	// much later on in the process; throws on failure
	check_repos_for_branch();

	// if the prefix exists, asks the user to delete it
	if (!check_clean_prefix())
		return 1;

	task_manager::instance().run_all();
	build_command::terminate_msbuild();

	prepare();
	make_bin();
	make_pdbs();
	make_src();
	make_uibase();
	make_installer();

	return 0;
}

void release_command::check_repos_for_branch()
{
	u8cout << "checking repos for branch " << branch_ << "...\n";

	thread_pool tp;
	std::atomic<bool> failed = false;

	for (const auto* t : task_manager::instance().find("super"))
	{
		if (!t->enabled())
			continue;

		tp.add([this, t, &failed]
		{
			const auto* o = dynamic_cast<const tasks::modorganizer*>(t);

			if (!git_wrap::remote_branch_exists(o->git_url(), branch_))
			{
				gcx().error(context::generic,
					"branch {} doesn't exist in the {} repo",
					branch_, o->name());

				failed = true;
			}
		});
	}

	tp.join();

	if (failed)
	{
		gcx().bail_out(context::generic,
			"either fix the branch name, create a remote branch for the "
			"repos that don't have it, or disable tasks with "
			"`-s TASKNAME:task/enabled=false`");
	}
}

bool release_command::check_clean_prefix()
{
	const auto prefix = conf().path().prefix();

	if (!fs::exists(prefix))
		return true;

	bool saw_file = false;
	const fs::path log_file = conf().global().get("log_file");
	const std::string ini_file = default_ini_filename();

	for (auto itor : fs::directory_iterator(prefix))
	{
		const auto name = itor.path().filename();

		// ignore ini and logs
		if (name == log_file.filename() || name == ini_file)
			continue;

		saw_file = true;
		break;
	}

	if (!saw_file)
	{
		// empty directory, that's fine
		return true;
	}

	const auto q = fmt::format(
		"prefix {} already exists, delete?", path_to_utf8(prefix));

	if (ask_yes_no(q, yn::no) != yn::yes)
		return false;

	// the log file might be in this directory, close it now and reopen it
	// when deletion is finished
	context::close_log_file();

	build_command::terminate_msbuild();
	op::delete_directory(gcx(), prefix);

	// reopen log file
	conf().set_log_file();

	return true;
}

void release_command::prepare()
{
	// finding rc file
	rc_path_ = fs::path(utf8_to_utf16(utf8_rc_path_));
	if (rc_path_.empty())
	{
		rc_path_ =
			tasks::modorganizer::super_path()
			/ "modorganizer"
			/ "src"
			/ "version.rc";
	}

	// getting version from rc or exe
	if (version_.empty())
	{
		if (version_rc_)
			version_ = version_from_rc();
		else
			version_ = version_from_exe();
	}

	// finding output path
	const auto prefix = conf().path().prefix();
	out_ = fs::path(utf8_to_utf16(utf8out_));

	if (out_.empty())
		out_ = prefix / "releases" / version_;
	else if (out_.is_relative())
		out_ = prefix / out_;
}

std::string release_command::do_doc()
{
	return
		"Creates archives for an MO installation, PDBs and sources.\n"
		"\n"
		"Commands:\n"
		"devbuild\n"
		"  Can creates three archives in `$prefix/releases/version`: one from\n"
		"  `install/bin/*`, one from `install/pdbs/*` and another with the\n"
		"  sources of projects from modorganizer_super.\n"
		"  \n"
		"  The archive filename is `Mod.Organizer-version-suffix-what.7z`,\n"
		"  where:\n"
		"    - `version` is taken from `ModOrganizer.exe`, `version.rc`\n"
		"      or from --version;\n"
		"    - `suffix` is the optional `--suffix` argument;\n"
		"    - `what` is either nothing, `src` or `pdbs`.\n"
		"\n"
		"official\n"
		"  Creates a new full build in the prefix. Requires that directory\n"
		"  to be empty. Puts the binary archive, source, PDBs and installer\n"
		"  in `$prefix/releases/version`. Forces all tasks to be enabled,\n"
		"  including translations and installer. Make sure the transifex API\n"
		"  key is in the INI or TX_TOKEN is set.";
}

std::string release_command::version_from_exe() const
{
	const auto exe = conf().path().install_bin() / "ModOrganizer.exe";

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

	const auto sub_block = fmt::format(
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

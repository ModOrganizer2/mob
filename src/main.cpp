#include "pch.h"
#include "conf.h"
#include "net.h"
#include "op.h"
#include "utility.h"
#include "tasks/tasks.h"
#include "tools/tools.h"

namespace mob
{

static std::vector<std::string> g_tasks_to_run;


std::string version()
{
	return "mob 1.0";
}

void show_help(const clipp::group& g)
{
	u8cout << clipp::make_man_page(
		g, "mob", clipp::doc_formatting()
		.first_column(4)
		.doc_column(30));
}

std::optional<int> handle_command_line(const std::vector<std::string>& args)
{
	struct
	{
		bool version = false;
		bool help = false;
		bool dry = false;
		bool redownload = false;
		bool reextract = false;
		bool rebuild = false;
		bool clean = false;
		bool list = false;
		int output_log_level = -1;
		int file_log_level = -1;
		std::string log_file;
		std::vector<std::string> options;
		std::vector<std::string> set;
		std::string ini;
		std::string prefix;
	} cmd;

	clipp::group g;

	g.push_back(
		(clipp::option("-v", "--version") >> cmd.version)
			% "shows the version",

		(clipp::option("-h", "--help") >> cmd.help)
			% "shows this message",

		(clipp::option("-d", "--destination")
			& clipp::value("DIR") >> cmd.prefix)
			% ("base output directory, will contain build/, install/, etc."),

		(clipp::option("-i", "--ini")
			& clipp::value("FILE") >> cmd.ini)
			% ("path to the ini file"),

		(clipp::option("--dry") >> cmd.dry)
			%  "simulates filesystem operations",

		(clipp::option("-l", "--log-level")
			&  clipp::value("LEVEL") >> cmd.output_log_level)
			%  "0 is silent, 6 is max",

		(clipp::option("--file-log-level")
			&  clipp::value("LEVEL") >> cmd.file_log_level)
				%  "overrides --log-level for the log file",

		(clipp::option("--log-file")
			&  clipp::value("FILE") >> cmd.log_file)
				%  "path to log file",

		(clipp::option("-g", "--redownload") >> cmd.redownload)
			% "redownloads archives, see --reextract",

		(clipp::option("-e", "--reextract") >> cmd.reextract)
			% "deletes source directories and re-extracts archives",

		(clipp::option("-b", "--rebuild") >> cmd.rebuild)
			%  "cleans and rebuilds projects",

		(clipp::option("-c", "--clean") >> cmd.clean)
			% "combines --redownload, --reextract and --rebuild",

		(clipp::repeatable(clipp::option("-s", "--set") >> cmd.set
			& clipp::opt_value("OPTION", cmd.options)))
			%  "sets an option, such as 'versions/openssl=1.2'; -s with no "
		       "arguments lists the available options",

		(clipp::option("--list") >> cmd.list)
			% "lists all the available tasks",

		(clipp::opt_values(
			clipp::match::prefix_not("-"), "task", g_tasks_to_run))
			% "tasks to run; specify 'super' to only build modorganizer "
		      "projects"
	);


	const auto pr = clipp::parse(args, g);

	if (!pr)
	{
		show_help(g);
		return 1;
	}

	if (cmd.version)
	{
		u8cout << version() << "\n";
		return 0;
	}

	if (cmd.help)
	{
		show_help(g);
		return 0;
	}

	if (cmd.list)
	{
		list_tasks();
		return 0;
	}

	if (!cmd.set.empty())
	{
		if (cmd.set.size() != cmd.options.size())
		{
			dump_available_options();
			return 0;
		}
	}

	if (cmd.file_log_level == -1)
		cmd.file_log_level = cmd.output_log_level;

	if (cmd.output_log_level >= 0)
	{
		cmd.options.push_back(
			"options/output_log_level=" +
			std::to_string(cmd.output_log_level));
	}

	if (cmd.file_log_level > 0)
	{
		cmd.options.push_back(
			"options/file_log_level=" +
			std::to_string(cmd.file_log_level));
	}

	if (!cmd.log_file.empty())
		cmd.options.push_back("options/log_file=" + cmd.log_file);

	if (cmd.dry)
		cmd.options.push_back("options/dry=true");

	if (cmd.redownload || cmd.clean)
		cmd.options.push_back("options/redownload=true");

	if (cmd.reextract || cmd.clean)
		cmd.options.push_back("options/reextract=true");

	if (cmd.rebuild || cmd.clean)
		cmd.options.push_back("options/rebuild=true");

	if (!cmd.prefix.empty())
		cmd.options.push_back("paths/prefix=" + cmd.prefix);

	init_options(cmd.ini, cmd.options);
	dump_options();

	return {};
}


BOOL WINAPI signal_handler(DWORD) noexcept
{
	gcx().debug(context::generic, "caught sigint");
	task::interrupt_all();
	return TRUE;
}

void add_tasks()
{
	add_task<sevenz>();
	add_task<zlib>();
	add_task<fmt>();
	add_task<gtest>();
	add_task<libbsarch>();
	add_task<libloot>();
	add_task<openssl>();
	add_task<libffi>();
	add_task<bzip2>();
	add_task<python>();
	add_task<boost>();
	add_task<boost_di>();
	add_task<lz4>();
	add_task<nmm>();
	add_task<ncc>();
	add_task<spdlog>();
	add_task<usvfs>();
	add_task<sip>();
	add_task<pyqt>();
	add_task<stylesheets>();
	add_task<licenses>();
	add_task<explorerpp>();

	add_task<parallel_tasks>(true)
		.add_task<modorganizer>("cmake_common")
		.add_task<modorganizer>("modorganizer-uibase");

	add_task<parallel_tasks>(true)
		.add_task<modorganizer>("modorganizer-game_features")
		.add_task<modorganizer>("modorganizer-archive")
		.add_task<modorganizer>("modorganizer-lootcli")
		.add_task<modorganizer>("modorganizer-esptk")
		.add_task<modorganizer>("modorganizer-bsatk")
		.add_task<modorganizer>("modorganizer-nxmhandler")
		.add_task<modorganizer>("modorganizer-helper")
		.add_task<modorganizer>("githubpp")
		.add_task<modorganizer>("modorganizer-game_gamebryo")
		.add_task<modorganizer>("modorganizer-bsapacker")
		.add_task<modorganizer>("modorganizer-preview_bsa");

	add_task<parallel_tasks>(true)
		.add_task<modorganizer>("modorganizer-game_oblivion")
		.add_task<modorganizer>("modorganizer-game_fallout3")
		.add_task<modorganizer>("modorganizer-game_fallout4")
		.add_task<modorganizer>("modorganizer-game_fallout4vr")
		.add_task<modorganizer>("modorganizer-game_falloutnv")
		.add_task<modorganizer>("modorganizer-game_morrowind")
		.add_task<modorganizer>("modorganizer-game_skyrim")
		.add_task<modorganizer>("modorganizer-game_skyrimse")
		.add_task<modorganizer>("modorganizer-game_skyrimvr")
		.add_task<modorganizer>("modorganizer-game_ttw")
		.add_task<modorganizer>("modorganizer-game_enderal");

	add_task<parallel_tasks>(true)
		.add_task<modorganizer>("modorganizer-tool_inieditor")
		.add_task<modorganizer>("modorganizer-tool_inibakery")
		.add_task<modorganizer>("modorganizer-preview_base")
		.add_task<modorganizer>("modorganizer-diagnose_basic")
		.add_task<modorganizer>("modorganizer-check_fnis")
		.add_task<modorganizer>("modorganizer-installer_bain")
		.add_task<modorganizer>("modorganizer-installer_manual")
		.add_task<modorganizer>("modorganizer-installer_bundle")
		.add_task<modorganizer>("modorganizer-installer_quick")
		.add_task<modorganizer>("modorganizer-installer_fomod")
		.add_task<modorganizer>("modorganizer-installer_ncc")
		.add_task<modorganizer>("modorganizer-bsa_extractor")
		.add_task<modorganizer>("modorganizer-plugin_python");

	add_task<parallel_tasks>(true)
		.add_task<modorganizer>("modorganizer-tool_configurator")
		.add_task<modorganizer>("modorganizer-fnistool")
		.add_task<modorganizer>("modorganizer-script_extender_plugin_checker")
		.add_task<modorganizer>("modorganizer-form43_checker")
		.add_task<modorganizer>("modorganizer-preview_dds");

	add_task<parallel_tasks>(true)
		.add_task<modorganizer>("modorganizer");
}

int run(const std::vector<std::string>& args)
{
	add_tasks();

	try
	{
		if (auto r=handle_command_line(args))
			return *r;
	}
	catch(bailed&)
	{
		// silent
		return 1;
	}


	try
	{

		::SetConsoleCtrlHandler(signal_handler, TRUE);

		curl_init curl;

		if (!g_tasks_to_run.empty())
			run_tasks(g_tasks_to_run);
		else
			run_all_tasks();

		return 0;
	}
	catch(bailed&)
	{
		error("bailing out");
		return 1;
	}
}

} // namespace


int wmain(int argc, wchar_t** argv)
{
	mob::set_std_streams();
	mob::set_thread_exception_handlers();

	std::vector<std::string> args;
	for (int i=1; i<argc; ++i)
		args.push_back(mob::utf16_to_utf8(argv[i]));

	int r = mob::run(args);

	if (r == 0)
	{
		mob::gcx().info(mob::context::generic, "mob done");
	}
	else
	{
		mob::gcx().info(mob::context::generic,
			"mob finished with exit code {}", r);
	}

	mob::dump_logs();

	return r;
}

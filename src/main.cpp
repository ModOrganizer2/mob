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
	std::cout
		<< clipp::make_man_page(
			g, "mob", clipp::doc_formatting()
			.first_column(4)
			.doc_column(30));
}

std::optional<int> handle_command_line(int argc, char** argv)
{
	struct
	{
		bool version = false;
		bool help = false;
		bool clean = false;
		int log = 3;
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

		(clipp::option("--dry")
			>> [&]{ cmd.set.push_back("conf/dry=true"); })
			%  "simulates filesystem operations",

		(clipp::option("-l", "--log-level")
			&  clipp::value("LEVEL") >> cmd.log)
			%  "0 is silent, 6 is max",

		(clipp::option("-g", "--redownload")
			>> [&]{ cmd.set.push_back("conf/redownload=true"); })
			% "redownloads archives, see --reextract",

		(clipp::option("-e", "--reextract")
			>> [&]{ cmd.set.push_back("conf/reextract=true"); })
			% "deletes source directories and re-extracts archives",

		(clipp::option("-b", "--rebuild")
			>> [&]{ cmd.set.push_back("conf/rebuild=true"); })
			%  "cleans and rebuilds projects",

		(clipp::option("-c", "--clean") >> cmd.clean)
			% "combines --redownload, --reextract and --rebuild",

		(clipp::repeatable(clipp::option("-s", "--set") >> cmd.set
			& clipp::opt_value("OPTION", cmd.options)))
			%  "sets an option, such as 'versions/openssl=1.2'; -s with no "
		       "arguments lists the available options",

		(clipp::opt_values(
			clipp::match::prefix_not("-"), "task", g_tasks_to_run))
			% "tasks to run"
	);


	const auto pr = clipp::parse(argc, argv, g);

	if (!pr)
	{
		show_help(g);
		return 1;
	}

	if (cmd.version)
	{
		std::cout << version() << "\n";
		return 0;
	}

	if (cmd.help)
	{
		show_help(g);
		return 0;
	}

	conf::set_log_level(cmd.log);

	if (!cmd.set.empty())
	{
		if (cmd.set.size() != cmd.options.size())
		{
			dump_available_options();
			return 0;
		}
	}

	if (cmd.clean)
	{
		cmd.options.push_back("conf/redownload=true");
		cmd.options.push_back("conf/reextract=true");
		cmd.options.push_back("conf/rebuild=true");
	}

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
	add_task<lz4>();
	add_task<nmm>();
	add_task<ncc>();
	add_task<spdlog>();
	add_task<usvfs>();
	add_task<sip>();
	add_task<pyqt>();
}

int run(int argc, char** argv)
{
	try
	{
		if (auto r=handle_command_line(argc, argv))
			return *r;

		::SetConsoleCtrlHandler(signal_handler, TRUE);

		curl_init curl;
		add_tasks();

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


int main(int argc, char** argv)
{
	int r = mob::run(argc, argv);

	mob::gcx().debug(mob::context::generic,
		"mob finished with exit code " + std::to_string(r));

	mob::dump_logs();

	return r;
}

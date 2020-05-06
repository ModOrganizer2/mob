#include "pch.h"
#include "conf.h"
#include "net.h"
#include "op.h"
#include "utility.h"
#include "tasks/tasks.h"
#include "tools/tools.h"

namespace mob
{


BOOL WINAPI signal_handler(DWORD) noexcept
{
	gcx().debug(context::generic, "caught sigint");
	task::interrupt_all();
	return TRUE;
}


struct curl_init
{
	curl_init()
	{
		curl_global_init(CURL_GLOBAL_ALL );
	}

	~curl_init()
	{
		curl_global_cleanup();
	}
};


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


int run(int argc, char** argv)
{
	struct
	{
		bool version = false;
		bool help = false;
	} cmd;

	clipp::group g;

	g.push_back(
		(clipp::option("--version")
			>> [&]{ cmd.version = true; })
			% "shows the version",

		(clipp::option("-h", "--help")
			>> [&]{ cmd.help = true; })
			% "shows this message"
	);


	conf_command_line_options(g);

	try
	{
		const auto pr = clipp::parse(argc, argv, g);

		if (!pr)
			throw bad_command_line();


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


		init_options();
		dump_options();
		return 0;

		::SetConsoleCtrlHandler(signal_handler, TRUE);

		curl_init curl;

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

		/*if (argc > 1)
		{
			std::vector<std::string> tasks;

			conf::set(argc, argv);

			for (int i=1; i<argc; ++i)
			{
				const std::string arg = argv[i];

				if (!arg.starts_with("--"))
					tasks.push_back(arg);
			}

			for (auto&& t : tasks)
			{
				if (!run_task(t))
					return 1;
			}

			return 0;
		}*/

		run_all_tasks();

		return 0;
	}
	catch(bad_command_line&)
	{
		show_help(g);
		return 1;
	}
	catch(bad_conf&)
	{
		return 1;
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
	//std::cin.get();
	return r;
}

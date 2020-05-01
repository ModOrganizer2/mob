#include "pch.h"
#include "conf.h"
#include "net.h"
#include "op.h"
#include "tools.h"
#include "utility.h"
#include "tasks/tasks.h"

namespace builder
{

BOOL WINAPI signal_handler(DWORD) noexcept
{
	info("caught sigint");
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


int run(int argc, char** argv)
{
	try
	{
		::SetConsoleCtrlHandler(signal_handler, TRUE);

		curl_init curl;
		vcvars();
		prepend_to_path(find_third_party_directory() / "bin");

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

		if (argc > 1)
		{
			std::vector<std::string> tasks;

			for (int i=1; i<argc; ++i)
			{
				const std::string arg = argv[i];

				if (arg == "--clean")
					conf::set_clean(true);
				else
					tasks.push_back(arg);
			}

			for (auto&& t : tasks)
			{
				if (!run_task(t))
					return 1;
			}

			return 0;
		}

		run_all_tasks();

		return 0;
	}
	catch(bailed)
	{
		error("bailing out");
		return 1;
	}
}

} // namespace


int main(int argc, char** argv)
{
	int r = builder::run(argc, argv);
	builder::dump_logs();
	//std::cin.get();
	return r;
}

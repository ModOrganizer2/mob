#include "pch.h"
#include "conf.h"
#include "net.h"
#include "op.h"
#include "utility.h"
#include "commands.h"
#include "tasks/tasks.h"
#include "tools/tools.h"

namespace mob
{

std::shared_ptr<command> handle_command_line(const std::vector<std::string>& args)
{
	auto help = std::make_shared<help_command>();
	auto build = std::make_shared<build_command>();

	std::vector<std::shared_ptr<command>> commands =
	{
		help,
		std::make_unique<version_command>(),
		std::make_unique<options_command>(),
		build,
		std::make_unique<list_command>(),
		std::make_unique<release_command>(),
		std::make_unique<git_command>(),
		std::make_unique<cmake_command>(),
		std::make_unique<inis_command>(),
		std::make_unique<tx_command>()
	};

	help->set_commands(commands);

	std::vector<clipp::group> command_groups;
	for (auto& c : commands)
		command_groups.push_back(c->group());

	clipp::group all_groups;
	all_groups.scoped(false);
	all_groups.exclusive(true);
	for (auto& c : command_groups)
		all_groups.push_back(c);

#pragma warning(suppress: 4548)
	auto cli = (all_groups, command::common_options_group());

	auto pr = clipp::parse(args, cli);

	if (!pr)
	{
		// if a command was picked, show its help instead of the main one
		for (auto&& c : commands)
		{
			if (c->picked())
			{
				c->force_help();
				return std::move(c);
			}
		}

		// bad command line
		help->force_exit_code(1);
		return help;
	}


	for (auto&& c : commands)
	{
		if (c->picked())
			return std::move(c);
	}

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
	add_task<parallel_tasks>(false)
		.add_task<sevenz>()
		.add_task<zlib>()
		.add_task<fmt>()
		.add_task<gtest>()
		.add_task<libbsarch>()
		.add_task<libloot>()
		.add_task<openssl>()
		.add_task<libffi>()
		.add_task<bzip2>()
		.add_task<nmm>();

	add_task<parallel_tasks>(false)
		.add_task<python>()
		.add_task<boost>()
		.add_task<boost_di>()
		.add_task<lz4>()
		.add_task<spdlog>();

	add_task<parallel_tasks>(false)
		.add_task<sip>()
		.add_task<ncc>();

	add_task<parallel_tasks>(false)
		.add_task<pyqt>()
		.add_task<usvfs>()
		.add_task<stylesheets>()
		.add_task<licenses>()
		.add_task<explorerpp>();


	using mo = modorganizer;

	// most of the alternate names below are from the transifex slugs, which
	// are sometimes different from the project names, for whatever reason

	add_task<parallel_tasks>(true)
		.add_task<mo>("cmake_common")
		.add_task<mo>("modorganizer-uibase");

	add_task<parallel_tasks>(true)
		.add_task<mo>("modorganizer-game_features")
		.add_task<mo>("modorganizer-archive")
		.add_task<mo>("modorganizer-lootcli")
		.add_task<mo>("modorganizer-esptk")
		.add_task<mo>("modorganizer-bsatk")
		.add_task<mo>("modorganizer-nxmhandler")
		.add_task<mo>("modorganizer-helper")
		.add_task<mo>("githubpp")
		.add_task<mo>("modorganizer-game_gamebryo")
		.add_task<mo>({"modorganizer-bsapacker", "bsa_packer"})
		.add_task<mo>("modorganizer-preview_bsa");

	// the gamebryo flag must be set for all game plugins that inherit from
	// the gamebryo classes; this will merge the .ts file from gamebryo with
	// the one from the specific plugin
	add_task<parallel_tasks>(true)
		.add_task<mo>("modorganizer-game_oblivion", mo::gamebryo)
		.add_task<mo>("modorganizer-game_fallout3", mo::gamebryo)
		.add_task<mo>("modorganizer-game_fallout4", mo::gamebryo)
		.add_task<mo>("modorganizer-game_fallout4vr", mo::gamebryo)
		.add_task<mo>("modorganizer-game_falloutnv", mo::gamebryo)
		.add_task<mo>("modorganizer-game_morrowind", mo::gamebryo)
		.add_task<mo>("modorganizer-game_skyrim", mo::gamebryo)
		.add_task<mo>("modorganizer-game_skyrimse", mo::gamebryo)
		.add_task<mo>("modorganizer-game_skyrimvr", mo::gamebryo)
		.add_task<mo>("modorganizer-game_ttw", mo::gamebryo)
		.add_task<mo>("modorganizer-game_enderal", mo::gamebryo);

	add_task<parallel_tasks>(true)
		.add_task<mo>({"modorganizer-tool_inieditor", "inieditor"})
		.add_task<mo>("modorganizer-tool_inibakery")
		.add_task<mo>("modorganizer-preview_base")
		.add_task<mo>("modorganizer-diagnose_basic")
		.add_task<mo>("modorganizer-check_fnis")
		.add_task<mo>("modorganizer-installer_bain")
		.add_task<mo>("modorganizer-installer_manual")
		.add_task<mo>("modorganizer-installer_bundle")
		.add_task<mo>("modorganizer-installer_quick")
		.add_task<mo>("modorganizer-installer_fomod")
		.add_task<mo>("modorganizer-installer_fomod_csharp")
		.add_task<mo>("modorganizer-installer_ncc")
		.add_task<mo>("modorganizer-bsa_extractor")
		.add_task<mo>("modorganizer-plugin_python")
		.add_task<translations>();

	add_task<parallel_tasks>(true)
		.add_task<mo>({"modorganizer-tool_configurator", "pycfg"})
		.add_task<mo>("modorganizer-fnistool")
		.add_task<mo>("modorganizer-basic_games")
		.add_task<mo>({
			"modorganizer-script_extender_plugin_checker",
			"diagnose-script_extender_plugin_checker",
			})
		.add_task<mo>({"modorganizer-form43_checker", "form43checker"})
		.add_task<mo>({"modorganizer-preview_dds", "ddspreview"})
		.add_task<mo>({"modorganizer", "organizer"});

	add_task<installer>();
}


// see https://github.com/isanae/mob/issues/4
//
// this restores the original console font if it changed
//
class font_restorer
{
public:
	font_restorer()
		: restore_(false)
	{
		std::memset(&old_, 0, sizeof(old_));
		old_.cbSize = sizeof(old_);

		if (GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &old_))
			restore_ = true;
	}

	~font_restorer()
	{
		if (!restore_)
			return;

		CONSOLE_FONT_INFOEX now = {};
		now.cbSize = sizeof(now);

		if (!GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &now))
			return;

		if (std::wcsncmp(old_.FaceName, now.FaceName, LF_FACESIZE) != 0)
			restore();
	}

	void restore()
	{
		::SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &old_);
	}

private:
	CONSOLE_FONT_INFOEX old_;
	bool restore_;
};

void set_sigint_handler()
{
	::SetConsoleCtrlHandler(mob::signal_handler, TRUE);
}


int run(const std::vector<std::string>& args)
{
	font_restorer fr;
	curl_init curl;

	add_tasks();

	try
	{
		auto c = handle_command_line(args);
		if (!c)
			return 1;

		return c->run();
	}
	catch(bailed&)
	{
		// silent
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
	mob::dump_logs();

	return r;
}

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
		.add_task<modorganizer>("modorganizer-installer_fomod_csharp")
		.add_task<modorganizer>("modorganizer-installer_ncc")
		.add_task<modorganizer>("modorganizer-bsa_extractor")
		.add_task<modorganizer>("modorganizer-plugin_python");

	add_task<parallel_tasks>(true)
		.add_task<modorganizer>("modorganizer-tool_configurator")
		.add_task<modorganizer>("modorganizer-fnistool")
		.add_task<modorganizer>("modorganizer-script_extender_plugin_checker")
		.add_task<modorganizer>("modorganizer-form43_checker")
		.add_task<modorganizer>("modorganizer-preview_dds")
		.add_task<modorganizer>("modorganizer");
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


int run(const std::vector<std::string>& args)
{
	font_restorer fr;

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
	::SetConsoleCtrlHandler(mob::signal_handler, TRUE);

	std::vector<std::string> args;
	for (int i=1; i<argc; ++i)
		args.push_back(mob::utf16_to_utf8(argv[i]));

	int r = mob::run(args);
	mob::dump_logs();

	return r;
}

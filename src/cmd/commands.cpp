#include "pch.h"
#include "commands.h"
#include "../utility.h"
#include "../net.h"
#include "../core/conf.h"
#include "../tasks/tasks.h"
#include "../tools/tools.h"
#include "../utility/threading.h"

namespace mob
{


BOOL WINAPI signal_handler(DWORD) noexcept
{
	gcx().debug(context::generic, "caught sigint");
	task::interrupt_all();
	return TRUE;
}

void set_sigint_handler()
{
	::SetConsoleCtrlHandler(mob::signal_handler, TRUE);
}


void help(const clipp::group& g, const std::string& more)
{
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
		<< "\n\n"
		<< "To use global options with command options, make sure command \n"
		<< "options are together, with no global options in the middle.\n";

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
	const auto master = default_ini_filename();

	return
		(clipp::repeatable(clipp::option("-i", "--ini")
			& clipp::value("FILE") >> o.inis))
			% "path to the ini file",

		(clipp::option("--dry") >> o.dry)
			% "simulates filesystem operations",

		(clipp::option("-l", "--log-level")
			& clipp::value("LEVEL") >> o.output_log_level)
			% "0 is silent, 6 is max",

		(clipp::option("--file-log-level")
			& clipp::value("LEVEL") >> o.file_log_level)
			% "overrides --log-level for the log file",

		(clipp::option("--log-file")
			& clipp::value("FILE") >> o.log_file)
			% "path to log file",

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

void command::force_help()
{
	help_ = true;
}

bool command::picked() const
{
	return picked_;
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

void command::set_task_enabled_flags(const std::vector<std::string>& names)
{
	common.options.push_back("task/enabled=false");

	bool failed = false;

	for (auto&& pattern : names)
		common.options.push_back(pattern + ":task/enabled=true");

	if (failed)
		throw bailed();
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
		const auto r = load_options();
		if (r != 0)
			return r;
	}

	if (flags_ & handle_sigint)
		set_sigint_handler();

	const auto r = do_run();

	if (code_)
		return *code_;

	return r;
}

int command::load_options()
{
	const int r = prepare_options(false);
	if (r != 0)
		return r;

	init_options(inis_, common.options);
	log_options();

	if (!verify_options())
		return 1;

	return 0;
}


command::meta_t version_command::meta() const
{
	return
	{
		"version",
		"shows the version"
	};
}

clipp::group version_command::do_group()
{
	return clipp::group(
		clipp::command("version", "-v", "--version").set(picked_));
}

int version_command::do_run()
{
	u8cout << mob_version() << "\n";
	return 0;
}


command::meta_t help_command::meta() const
{
	return
	{
		"help",
		"shows this message"
	};
}

void help_command::set_commands(const std::vector<std::shared_ptr<command>>& v)
{
	std::vector<std::pair<std::string, std::string>> s;

	for (auto&& c : v)
		s.push_back({c->meta().name, c->meta().description});

	commands_ = table(s, 4, 3);
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

	const auto master = default_ini_filename();

	help(doc,
		"Commands:\n"
		+ commands_ +
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
		"disable auto detection and only use --ini.");

	return 0;
}


options_command::options_command()
	: command(requires_options)
{
}

command::meta_t options_command::meta() const
{
	return
	{
		"options",
		"lists all options and their values from the inis"
	};
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


command::meta_t inis_command::meta() const
{
	return
	{
		"inis",
		"lists the INIs used by mob"
	};
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
	return prepare_options(true);
}

std::string inis_command::do_doc()
{
	return "Shows which INIs are found.";
}

}	// namespace

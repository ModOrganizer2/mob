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
	return "mob 1.0";
}


command::common_options command::common;

command::command()
	: picked_(false), code_(0)
{
}

clipp::group command::common_options_group()
{
	auto& o = common;

	return "Options" % (
		(clipp::option("-i", "--ini")
			& clipp::value("FILE") >> o.ini)
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

		(clipp::repeatable(clipp::option("-s", "--set") >> o.set
			& clipp::opt_value("OPTION", o.options)))
			%  "sets an option, such as 'versions/openssl=1.2'; -s with no "
			   "arguments lists the available options"
		);
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

int command::run()
{
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

	const auto r = do_run();

	if (code_)
		return *code_;

	return r;
}



clipp::group version_command::group()
{
	return clipp::group(
		"version command" % (
			(clipp::command("version", "-v", "--version")).set(picked_)
	));
}

int version_command::do_run()
{
	u8cout << version() << "\n";
	return 0;
}


clipp::group help_command::group()
{
	return clipp::group(
		"help command" % (
			(clipp::command("-h", "--help")).set(picked_)
	));
}

int help_command::do_run()
{
#pragma warning(suppress: 4548)
	auto doc = (command::common_options_group(), (clipp::value("command")));

	auto usage_df = clipp::doc_formatting()
		.first_column(4)
		.doc_column(30);

	auto options_df = clipp::doc_formatting()
		.first_column(0)
		.doc_column(30);

	u8cout
		<< "Usage:\n" << clipp::usage_lines(doc, "mob", usage_df)
		<< "\n\n"
		<< "Commands:\n"
		<< "    help       shows this message\n"
		<< "    version    shows the version\n"
		<< "    build      build tasks\n"
		<< "    list       lists available tasks\n"
		<< "    devbuild   create a devbuild\n"
		<< "\n"
		<< clipp::documentation(doc, options_df)
		<< "\n\n"
		<< "Invoking `mob` without a command builds everything. Do\n"
		<< "`mob build <task name>...` to build specific tasks. See\n"
		<< "`mob command --help` for more information about a command.\n";

	return 0;
}


clipp::group options_command::group()
{
	return clipp::group(
		"options command" % (
			(clipp::command("-h", "--help")).set(picked_)
	));
}

int options_command::do_run()
{
	dump_available_options();
	return 0;
}


clipp::group build_command::group()
{
	return "build command" % (
		(clipp::command("build")).set(picked_),

		(clipp::option("-g", "--redownload") >> redownload_)
			% "redownloads archives, see --reextract",

		(clipp::option("-e", "--reextract") >> reextract_)
			% "deletes source directories and re-extracts archives",

		(clipp::option("-b", "--rebuild") >> rebuild_)
			%  "cleans and rebuilds projects",

		(clipp::option("-c", "--clean") >> clean_)
			% "combines --redownload, --reextract and --rebuild",

		(clipp::opt_values(
			clipp::match::prefix_not("-"), "task", tasks_))
			% "tasks to run; specify 'super' to only build modorganizer "
			"projects"
	);
}

int build_command::do_run()
{
	try
	{
		if (redownload_ || clean_)
			common.options.push_back("options/redownload=true");

		if (reextract_ || clean_)
			common.options.push_back("options/reextract=true");

		if (rebuild_ || clean_)
			common.options.push_back("options/rebuild=true");


		init_options(command::common.ini, command::common.options);
		dump_options();

		if (!verify_options())
			return 1;

		curl_init curl;

		if (!tasks_.empty())
			run_tasks(tasks_);
		else
			run_all_tasks();

		mob::gcx().info(mob::context::generic, "mob done");
		return 0;
	}
	catch(bailed&)
	{
		error("bailing out");
		return 1;
	}
}


clipp::group list_command::group()
{
	return clipp::group(
		"list command" % (
			clipp::command("list").set(picked_)
	));
}

int list_command::do_run()
{
	list_tasks();
	return 0;
}


clipp::group devbuild_command::group()
{
	return clipp::group(
		"devbuild command" % (
			clipp::command("devbuild").set(picked_)
	));
}

int devbuild_command::do_run()
{
	verify_options();
	return 0;
}

}	// namespace

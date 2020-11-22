#include "pch.h"
#include "conf.h"
#include "context.h"
#include "env.h"
#include "ini.h"
#include "paths.h"
#include "../utility.h"
#include "../tasks/task.h"
#include "../tools/tools.h"

namespace mob::details
{

using key_value_map = std::map<std::string, std::string, std::less<>>;
using section_map = std::map<std::string, key_value_map, std::less<>>;
using task_map = std::map<std::string, section_map, std::less<>>;

static section_map g_conf;
static section_map g_tasks;

// special cases to avoid string manipulations
static int g_output_log_level = 3;
static int g_file_log_level = 5;
static bool g_dry = false;


bool bool_from_string(std::string_view s)
{
	return (s == "true" || s == "yes" || s == "1");
}

std::string get_string(std::string_view section, std::string_view key)
{
	auto sitor = g_conf.find(section);
	if (sitor == g_conf.end())
		gcx().bail_out(context::conf, "[{}] doesn't exist", section);

	auto kitor = sitor->second.find(key);
	if (kitor == sitor->second.end())
		gcx().bail_out(context::conf, "no key '{}' in [{}]", key, section);

	return kitor->second;
}

int get_int(std::string_view section, std::string_view key)
{
	const auto s = get_string(section, key);

	try
	{
		return std::stoi(s);
	}
	catch(std::exception&)
	{
		gcx().bail_out(context::conf, "bad int for {}/{}", section, key);
	}
}

bool get_bool(std::string_view section, std::string_view key)
{
	const auto s = get_string(section, key);
	return bool_from_string(s);
}

void set_string(std::string_view section, std::string_view key, std::string_view value)
{
	auto sitor = g_conf.find(section);
	if (sitor == g_conf.end())
		gcx().bail_out(context::conf, "[{}] doesn't exist", section);

	auto kitor = sitor->second.find(key);
	if (kitor == sitor->second.end())
		gcx().bail_out(context::conf, "no key '{}' [{}]", key, section);

	kitor->second = value;
}

void add_string(const std::string& section, const std::string& key, std::string value)
{
	g_conf[section][key] = value;
}

std::optional<std::string> find_string_for_task(
	std::string_view task_name, std::string_view key)
{
	// find task
	auto titor = g_tasks.find(task_name);
	if (titor == g_tasks.end())
		return {};

	const auto& task = titor->second;

	// find key
	auto itor = task.find(key);
	if (itor == task.end())
		return {};

	return itor->second;
}

std::string get_string_for_task(
	const std::vector<std::string>& task_names, std::string_view key)
{
	// there's a hierarchy for task options:
	//
	//  1) there's a special "_override" entry in g_tasks, for options set from
	//     the command line that should override everything, like --no-pull
	//     should override all pull settings for all tasks
	//
	//  2) if the key is not found in "_override", then there can be an entry
	//     in g_tasks with any of given task names
	//
	//  3) if there's no entry for the task, or the entry doesn't have the key,
	//     check if the task is a 'super' task (anything under
	//     modorganizer_super); g_tasks has another special entry 'super' for
	//     options that apply to all super tasks
	//
	//  4) if this is not a super task, or this key doesn't exist in the super
	//     section, then use the generic task option for it, stored in an
	//     element with an empty string in g_tasks


	// some command line options will override any user settings, like
	// --no-pull, those are stored in a special _override task name
	auto v = find_string_for_task("_override", key);
	if (v)
		return *v;

	// look for an option for this task by name
	for (auto&& tn : task_names)
	{
		v = find_string_for_task(tn, key);
		if (v)
			return *v;
	}

	// if any of the task names correspond to a super task, look for an option
	// for super
	for (auto&& tn : task_names)
	{
		if (is_super_task(tn))
		{
			v = find_string_for_task("super", key);
			if (v)
				return *v;

			break;
		}
	}

	// default task options are in a special empty string entry in g_tasks
	v = find_string_for_task("", key);
	if (v)
		return *v;

	// doesn't exist anywhere
	gcx().bail_out(context::conf,
		"no task option '{}' found for any of {}",
		key, join(task_names, ","));
}

bool get_bool_for_task(
	const std::vector<std::string>& task_names, std::string_view key)
{
	const std::string s = get_string_for_task(task_names, key);
	return bool_from_string(s);
}

void set_string_for_task(
	const std::string& task_name, const std::string& key, std::string value)
{
	// make sure the key exists, will throw if it doesn't
	get_string_for_task({task_name}, key);

	g_tasks[task_name][key] = std::move(value);
}

void add_string_for_task(
	const std::string& task_name, const std::string& key, std::string value)
{
	g_tasks[task_name][key] = std::move(value);
}

}	// namespace


namespace mob
{

std::vector<std::string> format_options()
{
	std::size_t longest_what = 0;
	std::size_t longest_key = 0;

	for (auto&& [section, kvs] : details::g_conf)
	{
		longest_what = std::max(longest_what, section.size());

		for (auto&& [k, v] : kvs)
			longest_key = std::max(longest_key, k.size());
	}

	for (auto&& [k, v] : details::g_tasks[""])
		longest_key = std::max(longest_key, k.size());

	for (const auto* task : get_all_tasks())
		longest_what = std::max(longest_what, task->name().size());

	std::vector<std::string> lines;

	lines.push_back(
		pad_right("what", longest_what) + "  " +
		pad_right("key",longest_key) + "   " +
		"value");

	lines.push_back(
		pad_right("-", longest_what, '-') + "  " +
		pad_right("-",longest_key, '-') + "   " +
		"-----");

	for (auto&& [section, kvs] : details::g_conf)
	{
		for (auto&& [k, v] : kvs)
		{
			lines.push_back(
				pad_right(section, longest_what) + "  " +
				pad_right(k, longest_key) + " = " + v);
		}
	}

	for (auto&& [k, v] : details::g_tasks[""])
	{
		lines.push_back(
			pad_right("task", longest_what) + "  " +
			pad_right(k, longest_key) + " = " + v);
	}

	for (const auto* t : get_all_tasks())
	{
		for (auto&& [k, unused] : details::g_tasks[""])
		{
			lines.push_back(
				pad_right(t->name(), longest_what) + "  " +
				pad_right(k, longest_key) + " = " +
				details::get_string_for_task({t->name()}, k));
		}
	}

	return lines;
}

void set_special_options()
{
	conf().global().set_output_log_level(
		details::get_string("global", "output_log_level"));

	conf().global().set_file_log_level(
		details::get_string("global", "file_log_level"));

	conf().global().set_dry(
		details::get_string("global", "dry"));
}

template <class F>
void set_path_if_empty(std::string_view k, F&& f)
{
	fs::path p = details::get_string("paths", k);

	if (p.empty())
	{
		if constexpr (std::is_same_v<fs::path, std::decay_t<decltype(f)>>)
			p = f;
		else
			p = f();
	}

	p = fs::absolute(p);

	if (!conf::dry())
	{
		if (!fs::exists(p))
			gcx().bail_out(context::conf, "path {} not found", p);

		p = fs::canonical(p);
	}

	details::set_string("paths", k, path_to_utf8(p));
}

void make_canonical_path(
	std::string_view key,
	const fs::path& default_parent, std::string_view default_dir)
{
	fs::path p = conf().path().get(key);

	if (p.empty())
	{
		p = default_parent / default_dir;
	}
	else
	{
		if (p.is_relative())
			p = default_parent / p;
	}

	if (!conf::dry())
		p = fs::weakly_canonical(fs::absolute(p));

	details::set_string("paths", key, path_to_utf8(p));
}

struct parsed_option
{
	std::string section, key, value;
};

parsed_option parse_option(const std::string& s)
{
	// parses "section/key=value"
	static std::regex re(R"((.+)/(.+)=(.*))");
	std::smatch m;

	if (!std::regex_match(s, m, re))
	{
		gcx().bail_out(context::conf,
			"bad option {}, must be [task:]section/key=value", s);
	}

	return {
		trim_copy(m[1].str()),
		trim_copy(m[2].str()),
		trim_copy(m[3].str())
	};
}

void process_option(
	const std::string& section_string,
	const std::string& key, const std::string& value, bool master)
{
	const auto col = section_string.find(":");
	std::string task, section;

	if (col == std::string::npos)
	{
		section = section_string;
	}
	else
	{
		task = section_string.substr(0, col);
		section = section_string.substr(col + 1);
	}

	if (section == "task")
	{
		if (task == "_override")
		{
			details::set_string_for_task("_override", key, value);
		}
		else if (task != "")
		{
			const auto& tasks = find_tasks(task);
			MOB_ASSERT(!tasks.empty());

			for (auto& t : tasks)
				details::set_string_for_task(t->name(), key, value);
		}
		else
		{
			if (master)
				details::add_string_for_task("", key, value);
			else
				details::set_string_for_task("", key, value);
		}
	}
	else
	{
		if (master)
			details::add_string(section, key, value);
		else
			details::set_string(section, key, value);
	}
}

void process_ini(const fs::path& ini, bool master)
{
	const auto data = parse_ini(ini);

	for (auto&& a : data.aliases)
		add_alias(a.first, a.second);

	for (auto&& [section_string, kvs] : data.sections)
	{
		for (auto&& [k, v] : kvs)
			process_option(section_string, k, v, master);
	}
}

void process_cmd_options(const std::vector<std::string>& opts)
{
	gcx().debug(context::conf, "overriding from command line:");

	for (auto&& o : opts)
	{
		const auto po = parse_option(o);
		process_option(po.section, po.key, po.value, false);
	}
}

void init_options(
	const std::vector<fs::path>& inis, const std::vector<std::string>& opts)
{
	MOB_ASSERT(!inis.empty());

	// Keep track of the INI that contained a prefix:
	fs::path ini_prefix;
	bool master = true;

	for (auto&& ini : inis)
	{
		// Check if the prefix is set by this ini file:
		fs::path cprefix = master ? fs::path{} : conf().path().prefix();

		process_ini(ini, master);

		if (conf().path().prefix() != cprefix)
			ini_prefix = ini;

		master = false;
	}


	if (!opts.empty())
	{
		const auto prefix_before = conf().path().prefix();

		process_cmd_options(opts);

		if (conf().path().prefix() != prefix_before)
		{
			// overridden by command line
			ini_prefix = "";
		}
	}


	set_special_options();

	if (!conf().path().prefix().empty())
		make_canonical_path("prefix", ini_prefix.empty() ? fs::current_path() : ini_prefix.parent_path(), "");

	auto log_file = conf().global().log_file();
	if (log_file.is_relative())
		log_file = conf().path().prefix() / log_file;

	context::set_log_file(log_file);

	gcx().debug(context::conf,
		"command line: {}", std::wstring(GetCommandLineW()));

	gcx().debug(context::conf, "using inis in order:");

	for (auto&& ini : inis)
		gcx().debug(context::conf, "  . {}", ini);

	set_path_if_empty("third_party", find_third_party_directory);
	this_env::prepend_to_path(conf().path().third_party() / "bin");

	set_path_if_empty("pf_x86",     find_program_files_x86);
	set_path_if_empty("pf_x64",     find_program_files_x64);
	set_path_if_empty("vs",         find_vs);
	set_path_if_empty("qt_install", find_qt);
	set_path_if_empty("temp_dir",   find_temp_dir);
	set_path_if_empty("patches",    find_in_root("patches"));
	set_path_if_empty("licenses",   find_in_root("licenses"));
	set_path_if_empty("qt_bin",     qt::installation_path() / "bin");

	details::set_string("tools", "vcvars", path_to_utf8(find_vcvars()));

	this_env::append_to_path(conf().path().get("qt_bin"));

	make_canonical_path("cache",             conf().path().prefix(), "downloads");
	make_canonical_path("build",             conf().path().prefix(), "build");
	make_canonical_path("install",           conf().path().prefix(), "install");
	make_canonical_path("install_installer", conf().path().install(), "installer");
	make_canonical_path("install_bin",       conf().path().install(), "bin");
	make_canonical_path("install_libs",      conf().path().install(), "libs");
	make_canonical_path("install_pdbs",      conf().path().install(), "pdb");
	make_canonical_path("install_dlls",      conf().path().install_bin(), "dlls");
	make_canonical_path("install_loot",      conf().path().install_bin(), "loot");
	make_canonical_path("install_plugins",   conf().path().install_bin(), "plugins");
	make_canonical_path("install_licenses",  conf().path().install_bin(), "licenses");

	make_canonical_path(
		"install_pythoncore",
		conf().path().install_dlls(), "pythoncore");

	make_canonical_path(
		"install_stylesheets",
		conf().path().install_bin(), "stylesheets");

	make_canonical_path(
		"install_translations",
		conf().path().install_bin(), "resources/translations");

	details::set_string("tools", "iscc", path_to_utf8(find_iscc()));
}

bool verify_options()
{
	if (conf().path().prefix().empty())
	{
		u8cerr
			<< "missing prefix; either specify it the [paths] section of "
			<< "the ini or pass '-d path'\n";

		return false;
	}

	// will be created later if it doesn't exist
	if (fs::exists(conf().path().prefix()))
	{
		if (fs::equivalent(conf().path().prefix(), mob_exe_path().parent_path()))
		{
			u8cerr
				<< "the prefix cannot be where mob.exe is, there's already a "
				<< "build directory in there\n";

			return false;
		}
	}

	return true;
}

void log_options()
{
	for (auto&& line : format_options())
		gcx().trace(context::conf, "{}", line);
}

void dump_available_options()
{
	for (auto&& line : format_options())
		u8cout << line << "\n";
}


conf_global conf::global()
{
	return {};
}

conf_task conf::task(const std::vector<std::string>& names)
{
	return {names};
}

conf_tools conf::tool()
{
	return {};
}

conf_transifex conf::transifex()
{
	return {};
}

conf_prebuilt conf::prebuilt()
{
	return {};
}

conf_versions conf::version()
{
	return {};
}

conf_paths conf::path()
{
	return {};
}

bool conf::dry()
{
	return conf().global().dry();
}


conf_global::conf_global()
	: conf_section("global")
{
}

int conf_global::output_log_level() const
{
	return details::g_output_log_level;
}

void conf_global::set_output_log_level(const std::string& s)
{
	if (s.empty())
		return;

	try
	{
		const auto i = std::stoi(s);

		if (i < 0 || i > 6)
			gcx().bail_out(context::generic, "bad output log level {}", i);

		details::g_output_log_level = i;
	}
	catch(std::exception&)
	{
		gcx().bail_out(context::generic, "bad output log level {}", s);
	}
}

int conf_global::file_log_level() const
{
	return details::g_file_log_level;
}

void conf_global::set_file_log_level(const std::string& s)
{
	if (s.empty())
		return;

	try
	{
		const auto i = std::stoi(s);
		if (i < 0 || i > 6)
			gcx().bail_out(context::generic, "bad file log level {}", i);

		details::g_file_log_level = i;
	}
	catch(std::exception&)
	{
		gcx().bail_out(context::generic, "bad file log level {}", s);
	}
}

bool conf_global::dry() const
{
	return details::g_dry;
}

void conf_global::set_dry(std::string_view s)
{
	details::g_dry = details::bool_from_string(s);
}


conf_task::conf_task(std::vector<std::string> names)
	: names_(std::move(names))
{
}

std::string conf_task::get(std::string_view key) const
{
	return details::get_string_for_task(names_, key);
}

bool conf_task::get_bool(std::string_view key) const
{
	return details::get_bool_for_task(names_, key);
}


conf_tools::conf_tools()
	: conf_section("tools")
{
}

conf_transifex::conf_transifex()
	: conf_section("transifex")
{
}

conf_versions::conf_versions()
	: conf_section("versions")
{
}

conf_prebuilt::conf_prebuilt()
	: conf_section("prebuilt")
{
}

conf_paths::conf_paths()
	: conf_section("paths")
{
}

}	// namespace

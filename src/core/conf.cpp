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
static task_map g_map;


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
	auto global = g_map.find("");
	MOB_ASSERT(global != g_map.end());

	auto sitor = global->second.find(section);
	if (sitor == global->second.end())
	{
		gcx().bail_out(context::conf,
			"conf section '{}' doesn't exist", section);
	}

	auto kitor = sitor->second.find(key);
	if (kitor == sitor->second.end())
	{
		gcx().bail_out(context::conf,
			"key '{}' not found in section '{}'", key, section);
	}

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

void set_string(
	std::string_view section, std::string_view key,
	std::string_view value)
{
	auto global = g_map.find("");
	MOB_ASSERT(global != g_map.end());

	auto sitor = global->second.find(section);
	if (sitor == global->second.end())
	{
		gcx().bail_out(context::conf,
			"conf section '{}' doesn't exist", section);
	}

	auto kitor = sitor->second.find(key);
	if (kitor == sitor->second.end())
	{
		gcx().bail_out(context::conf,
			"key '{}' not found in section '{}'", key, section);
	}

	kitor->second = value;
}

void add_string(
	std::string_view section, std::string_view key, std::string_view value)
{
	g_map[""][std::string(section)][std::string(key)] = value;
}

std::optional<std::string> find_string_for_task(
	std::string_view task_name,
	std::string_view section_name, std::string_view key)
{
	auto titor = g_map.find(task_name);
	if (titor == g_map.end())
		return {};

	const auto& task = titor->second;

	auto sitor = task.find(section_name);
	if (sitor == task.end())
		return {};

	const auto& section = sitor->second;

	auto itor = section.find(key);
	if (itor == section.end())
		return {};

	return itor->second;
}

std::string get_string_for_task(
	const std::vector<std::string>& task_names,
	std::string_view section, std::string_view key)
{
	task_map::iterator task = g_map.end();

	auto v = find_string_for_task("_override", section, key);
	if (v)
		return *v;

	for (auto&& tn : task_names)
	{
		v = find_string_for_task(tn, section, key);
		if (v)
			return *v;
	}

	for (auto&& tn : task_names)
	{
		if (is_super_task(tn))
		{
			v = find_string_for_task("super", section, key);
			if (v)
				return *v;

			break;
		}
	}

	return get_string(section, key);
}

bool get_bool_for_task(
	const std::vector<std::string>& task_names,
	std::string_view section, std::string_view key)
{
	const std::string s = get_string_for_task(task_names, section, key);
	return bool_from_string(s);
}

void set_string_for_task(
	std::string_view task_name, std::string_view section,
	std::string_view key, std::string_view value)
{
	// make sure the key exists, will throw if it doesn't
	get_string(section, key);

	g_map[std::string(task_name)][std::string(section)][std::string(key)] = value;
}

}	// namespace


namespace mob
{

std::vector<std::string> format_options()
{
	std::size_t longest_task = 0;
	std::size_t longest_section = 0;
	std::size_t longest_key = 0;

	for (auto&& [t, ss] : details::g_map)
	{
		longest_task = std::max(longest_task, t.size());

		for (auto&& [s, kv] : ss)
		{
			longest_section = std::max(longest_section, s.size());

			for (auto&& [k, v] : kv)
				longest_key = std::max(longest_key, k.size());
		}
	}

	std::vector<std::string> lines;

	lines.push_back(
		pad_right("task", longest_task) + "  " +
		pad_right("section", longest_section) + "  " +
		pad_right("key",longest_key) + "   " +
		"value");

	lines.push_back(
		pad_right("-", longest_task, '-') + "  " +
		pad_right("-", longest_section, '-') + "  " +
		pad_right("-",longest_key, '-') + "   " +
		"-----");

	for (auto&& [t, ss] : details::g_map)
	{
		for (auto&& [s, kv] : ss)
		{
			for (auto&& [k, v] : kv)
			{
				lines.push_back(
					pad_right(t, longest_task) + "  " +
					pad_right(s, longest_section) + "  " +
					pad_right(k, longest_key) + " = " + v);
			}
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
	std::string task, section, key, value;
};

parsed_option parse_option(const std::string& s)
{
	// parses "task:section/key=value" where "task:" is optional
	static std::regex re(R"((?:(.+)\:)?(.+)/(.*)=(.*))");
	std::smatch m;

	if (!std::regex_match(s, m, re))
	{
		gcx().bail_out(context::conf,
			"bad option {}, must be [task:]section/key=value", s);
	}

	return {
		trim_copy(m[1].str()),
		trim_copy(m[2].str()),
		trim_copy(m[3].str()),
		trim_copy(m[4].str())
	};
}

void init_options(
	const std::vector<fs::path>& inis, const std::vector<std::string>& opts)
{
	MOB_ASSERT(!inis.empty());

	// Keep track of the INI that contained a prefix:
	fs::path ini_prefix;
	bool add = true;
	for (auto&& ini : inis)
	{
		// Check if the prefix is set by this ini file:
		fs::path cprefix = add ? fs::path{} : conf().path().prefix();
		const auto data = parse_ini(ini);

		for (auto&& a : data.aliases)
			add_alias(a.first, a.second);

		for (auto&& [section_string, kvs] : data.sections)
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

			for (auto&& [k, v] : kvs)
			{
				if (task.empty())
				{
					if (add)
						details::add_string(section, k, v);
					else
						details::set_string(section, k, v);
				}
				else
				{
					if (task == "_override")
					{
						details::set_string_for_task("_override", section, k, v);
					}
					else
					{
						const auto& tasks = find_tasks(task);
						MOB_ASSERT(!tasks.empty());

						for (auto& t : tasks)
							details::set_string_for_task(t->name(), section, k, v);
					}
				}
			}
		}

		if (conf().path().prefix() != cprefix)
			ini_prefix = ini;

		add = false;
	}

	if (!opts.empty())
	{
		gcx().debug(context::conf, "overriding from command line:");

		for (auto&& o : opts)
		{
			const auto po = parse_option(o);

			if (po.section == "paths" && po.key == "prefix")
			{
				ini_prefix = "";
			}

			if (po.task.empty())
			{
				details::set_string(po.section, po.key, po.value);
			}
			else
			{
				if (po.task == "_override")
				{
					details::set_string_for_task(
						"_override", po.section, po.key, po.value);
				}
				else
				{
					const auto& tasks = find_tasks(po.task);

					if (tasks.empty())
					{
						gcx().bail_out(context::generic,
							"no task matching '{}' found (command line option)",
							po.task);
					}

					for (auto& t : tasks)
					{
						details::set_string_for_task(
							t->name(), po.section, po.key, po.value);
					}
				}
			}
		}
	}

	set_special_options();

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

	find_vcvars();
	validate_qt();

	this_env::append_to_path(conf().path().get("qt_bin"));

	if (!conf().path().prefix().empty())
		make_canonical_path("prefix", ini_prefix.empty() ? fs::current_path() : ini_prefix.parent_path(), "");

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
	return details::get_string_for_task(names_, "task", key);
}

bool conf_task::get_bool(std::string_view key) const
{
	return details::get_bool_for_task(names_, "task", key);
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

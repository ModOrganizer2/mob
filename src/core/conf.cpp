#include "pch.h"
#include "conf.h"
#include "context.h"
#include "env.h"
#include "ini.h"
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

fs::path find_iscc()
{
	const auto tasks = find_tasks("installer");
	MOB_ASSERT(tasks.size() == 1);

	if (!tasks[0]->enabled())
		return {};

	auto iscc = conf().tool().get("iscc");
	if (iscc.is_absolute())
	{
		if (!fs::exists(iscc))
		{
			gcx().bail_out(context::conf,
				"{} doesn't exist (from ini, absolute path)", iscc);
		}

		return iscc;
	}

	fs::path p = find_in_path(path_to_utf8(iscc));
	if (fs::exists(p))
		return fs::canonical(fs::absolute(p));

	for (int v : {5, 6, 7, 8})
	{
		const fs::path inno = fmt::format("inno setup {}", v);

		for (fs::path pf : {conf().path().pf_x86(), conf().path().pf_x64()})
		{
			p = pf / inno / iscc;
			if (fs::exists(p))
				return fs::canonical(fs::absolute(p));
		}
	}

	gcx().bail_out(context::conf, "can't find {} anywhere", iscc);
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

bool try_parts(fs::path& check, const std::vector<std::string>& parts)
{
	for (std::size_t i=0; i<parts.size(); ++i)
	{
		fs::path p = check;

		for (std::size_t j=i; j<parts.size(); ++j)
			p /= parts[j];

		gcx().trace(context::conf, "trying parts {}", p);

		if (fs::exists(p))
		{
			check = p;
			return true;
		}
	}

	return false;
}

fs::path mob_exe_path()
{
	// double the buffer size 10 times
	const int max_tries = 10;

	DWORD buffer_size = MAX_PATH;

	for (int tries=0; tries<max_tries; ++tries)
	{
		auto buffer = std::make_unique<wchar_t[]>(buffer_size + 1);
		DWORD n = GetModuleFileNameW(0, buffer.get(), buffer_size);

		if (n == 0) {
			const auto e = GetLastError();
			gcx().bail_out(context::conf,
				"can't get module filename, {}", error_message(e));
		}
		else if (n >= buffer_size) {
			// buffer is too small, try again
			buffer_size *= 2;
		} else {
			// if GetModuleFileName() works, `n` does not include the null
			// terminator
			const std::wstring s(buffer.get(), n);
			return fs::canonical(s);
		}
	}

	gcx().bail_out(context::conf, "can't get module filename");
}

fs::path find_root(bool verbose)
{
	gcx().trace(context::conf, "looking for root directory");

	fs::path mob_exe_dir = mob_exe_path().parent_path();

	auto third_party = mob_exe_dir / "third-party";

	if (!fs::exists(third_party))
	{
		if (verbose)
			u8cout << "no third-party here\n";

		auto p = mob_exe_dir;

		if (p.filename().u8string() == u8"x64")
		{
			p = p.parent_path();
			if (p.filename().u8string() == u8"Debug" ||
				p.filename().u8string() == u8"Release")
			{
				if (verbose)
					u8cout << "this is mob's build directory, looking up\n";

				// mob_exe_dir is in the build directory
				third_party = mob_exe_dir / ".." / ".." / ".." / "third-party";
			}
		}
	}

	if (!fs::exists(third_party))
		gcx().bail_out(context::conf, "root directory not found");

	const auto p = fs::canonical(third_party.parent_path());
	gcx().trace(context::conf, "found root directory at {}", p);
	return p;
}

fs::path find_in_root(const fs::path& file)
{
	static fs::path root = find_root();

	fs::path p = root / file;
	if (!fs::exists(p))
		gcx().bail_out(context::conf, "{} not found", p);

	gcx().trace(context::conf, "found {}", p);
	return p;
}


fs::path find_third_party_directory()
{
	static fs::path path = find_in_root("third-party");
	return path;
}


fs::path find_in_path(std::string_view exe)
{
	const std::wstring wexe = utf8_to_utf16(exe);

	const std::size_t size = MAX_PATH;
	wchar_t buffer[size + 1] = {};

	if (SearchPathW(nullptr, wexe.c_str(), nullptr, size, buffer, nullptr))
		return buffer;
	else
		return {};
}

bool find_qmake(fs::path& check)
{
	// try Qt/Qt5.14.2/msvc*/bin/qmake.exe
	if (try_parts(check, {
		"Qt",
		"Qt" + qt::version(),
		"msvc" + qt::vs_version() + "_64",
		"bin",
		"qmake.exe"}))
	{
		return true;
	}

	// try Qt/5.14.2/msvc*/bin/qmake.exe
	if (try_parts(check, {
		"Qt",
		qt::version(),
		"msvc" + qt::vs_version() + "_64",
		"bin",
		"qmake.exe"}))
	{
		return true;
	}

	return false;
}

bool try_qt_location(fs::path& check)
{
	if (!find_qmake(check))
		return false;

	check = fs::absolute(check.parent_path() / "..");
	return true;
}

fs::path find_qt()
{
	fs::path p = conf().path().qt_install();

	if (!p.empty())
	{
		p = fs::absolute(p);

		if (!try_qt_location(p))
			gcx().bail_out(context::conf, "no qt install in {}", p);

		return p;
	}


	std::vector<fs::path> locations =
	{
		conf().path().pf_x64(),
		"C:\\",
		"D:\\"
	};

	// look for qmake, which is in %qt%/version/msvc.../bin
	fs::path qmake = find_in_path("qmake.exe");
	if (!qmake.empty())
		locations.insert(locations.begin(), qmake.parent_path() / "../../");

	// look for qtcreator.exe, which is in %qt%/Tools/QtCreator/bin
	fs::path qtcreator = find_in_path("qtcreator.exe");
	if (!qtcreator.empty())
		locations.insert(locations.begin(), qtcreator.parent_path() / "../../../");

	for (fs::path loc : locations)
	{
		loc = fs::absolute(loc);
		if (try_qt_location(loc))
			return loc;
	}

	gcx().bail_out(context::conf, "can't find qt install");
}

void validate_qt()
{
	fs::path p = qt::installation_path();

	if (!try_qt_location(p))
		gcx().bail_out(context::conf, "qt path {} doesn't exist", p);

	details::set_string("paths", "qt_install", path_to_utf8(p));
}

fs::path get_known_folder(const GUID& id)
{
	wchar_t* buffer = nullptr;
	const auto r = ::SHGetKnownFolderPath(id, 0, 0, &buffer);

	if (r != S_OK)
		return {};

	fs::path p = buffer;
	::CoTaskMemFree(buffer);

	return p;
}

fs::path find_program_files_x86()
{
	fs::path p = get_known_folder(FOLDERID_ProgramFilesX86);

	if (p.empty())
	{
		const auto e = GetLastError();

		p = fs::path(R"(C:\Program Files (x86))");

		gcx().warning(context::conf,
			"failed to get x86 program files folder, defaulting to {}, {}",
			p, error_message(e));
	}
	else
	{
		gcx().trace(context::conf, "x86 program files is {}", p);
	}

	return p;
}

fs::path find_program_files_x64()
{
	fs::path p = get_known_folder(FOLDERID_ProgramFilesX64);

	if (p.empty())
	{
		const auto e = GetLastError();

		p = fs::path(R"(C:\Program Files)");

		gcx().warning(context::conf,
			"failed to get x64 program files folder, defaulting to {}, {}",
			p, error_message(e));
	}
	else
	{
		gcx().trace(context::conf, "x64 program files is {}", p);
	}

	return p;
}

fs::path find_temp_dir()
{
	const std::size_t buffer_size = MAX_PATH + 2;
	wchar_t buffer[buffer_size] = {};

	if (GetTempPathW(static_cast<DWORD>(buffer_size), buffer) == 0)
	{
		const auto e = GetLastError();
		gcx().bail_out(context::conf, "can't get temp path", error_message(e));
	}

	fs::path p(buffer);
	gcx().trace(context::conf, "temp dir is {}", p);

	return p;
}

fs::path find_vs()
{
	if (conf::dry())
		return vs::vswhere();

	const auto path = vswhere::find_vs();
	if (path.empty())
		gcx().bail_out(context::conf, "vswhere failed");

	const auto lines = split(path_to_utf8(path), "\r\n");

	if (lines.empty())
	{
		gcx().bail_out(context::conf, "vswhere didn't output anything");
	}
	else if (lines.size() > 1)
	{
		gcx().error(context::conf, "vswhere returned multiple installations:");

		for (auto&& line : lines)
			gcx().error(context::conf, " - {}", line);

		gcx().bail_out(context::conf,
			"specify the `vs` path in the `[paths]` section of the INI, or "
			"pass -s paths/vs=PATH` to pick an installation");
	}

	if (!fs::exists(path))
	{
		gcx().bail_out(context::conf,
			"the path given by vswhere doesn't exist: {}", path);
	}

	return path;
}

bool try_vcvars(fs::path& bat)
{
	if (!fs::exists(bat))
		return false;

	bat = fs::canonical(fs::absolute(bat));
	return true;
}

void find_vcvars()
{
	fs::path bat = conf().tool().get("vcvars");

	if (conf::dry())
	{
		if (bat.empty())
			bat = "vcvars.bat";

		return;
	}
	else
	{
		if (bat.empty())
		{
			bat = vs::installation_path()
				/ "VC" / "Auxiliary" / "Build" / "vcvarsall.bat";

			if (!try_vcvars(bat))
				gcx().bail_out(context::conf, "vcvars not found at {}", bat);
		}
		else
		{
			if (!try_vcvars(bat))
				gcx().bail_out(context::conf, "vcvars not found at {}", bat);
		}
	}

	details::set_string("tools", "vcvars", path_to_utf8(bat));
	gcx().trace(context::conf, "using vcvars at {}", bat);
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
		parse_ini(ini, add);

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


fs::path make_temp_file()
{
	static fs::path dir = conf().path().temp_dir();

	wchar_t name[MAX_PATH + 1] = {};
	if (GetTempFileNameW(dir.native().c_str(), L"mob", 0, name) == 0)
	{
		const auto e = GetLastError();

		gcx().bail_out(context::conf,
			"can't create temp file in {}, {}", dir, error_message(e));
	}

	return dir / name;
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

#include "pch.h"
#include "conf.h"
#include "utility.h"
#include "context.h"
#include "process.h"

namespace mob
{

using string_map = std::map<std::string, std::string>;
using path_map = std::map<std::string, fs::path>;
using bool_map   = std::map<std::string, bool>;

static fs::path g_ini;
const std::string default_ini_filename = "mob.ini";

// special case to avoid string manipulations
static int g_log = 3;

static string_map g_options =
{
	{"mo_org",     "ModOrganizer2"},
	{"mo_branch",  "master"},
	{"dry",        "false"},
	{"redownload", "false"},
	{"reextract",  "false"},
	{"rebuild",    "false"}
};

static path_map g_tools =
{
	{"sevenz",  "7z.exe"},
	{"jom",     "jom.exe"},
	{"patch",   "patch.exe"},
	{"nuget",   "nuget.exe"},
	{"vswhere", "vswhere.exe"},
	{"perl",    "perl.exe"},
	{"msbuild", "msbuild.exe"},
	{"devenv",  "devenv.exe"},
	{"cmake",   "cmake.exe"},
	{"git",     "git.exe"},
	{"vcvars",   ""},
};

static bool_map g_prebuilt =
{
	//{"boost",   false}
};

static string_map g_versions =
{
	{"vs",           ""},
	{"vs_year",      ""},
	{"vs_toolset",   ""},
	{"sdk",          ""},
	{"sevenzip",     ""},
	{"zlib",         ""},
	{"boost",        ""},
	{"boost_vs",     ""},
	{"python",       ""},
	{"fmt",          ""},
	{"gtest",        ""},
	{"libbsarch",    ""},
	{"libloot",      ""},
	{"libloot_hash", ""},
	{"openssl",      ""},
	{"bzip2",        ""},
	{"lz4",          ""},
	{"nmm",          ""},
	{"spdlog",       ""},
	{"usvfs",        ""},
	{"qt",           ""},
	{"qt_vs",        ""},
	{"pyqt",         ""},
	{"pyqt_builder", ""},
	{"sip",          ""},
	{"pyqt_sip",     ""},

	{"ss_6788_paper_lad",      ""},
	{"ss_6788_paper_automata", ""},
	{"ss_6788_paper_mono",     ""},
	{"ss_6788_1809_dark_mode", ""}
};

static path_map g_paths =
{
	{"third_party" ,        ""},
	{"prefix",              ""},
	{"cache",               ""},
	{"build",               ""},
	{"install",             ""},
	{"install_bin",         ""},
	{"install_libs",        ""},
	{"install_pdbs",        ""},
	{"install_dlls",        ""},
	{"install_loot",        ""},
	{"install_plugins",     ""},
	{"install_stylesheets", ""},
	{"install_licenses",    ""},
	{"install_pythoncore",  ""},
	{"patches",             ""},
	{"licenses",            ""},
	{"vs",                  ""},
	{"qt_install",          ""},
	{"qt_bin",              ""},
	{"pf_x86",              ""},
	{"pf_x64",              ""},
	{"temp_dir",            ""},
};


template <class Map>
const typename Map::mapped_type& get(
	const std::string& map_name, const Map& map,
	const std::string& name)
{
	auto itor = map.find(name);

	if (itor == map.end())
	{
		gcx().bail_out(context::conf,
			map_name + " '" + name + "' doesn't exist");
	}

	return itor->second;
}


const fs::path& tools::by_name(const std::string& name)
{
	return get("tool", g_tools, name);
}

const std::string& conf::by_name(const std::string& name)
{
	return get("option", g_options, name);
}

bool conf::by_name_bool(const std::string& name)
{
	bool b;
	std::istringstream iss(by_name(name));
	iss >> std::boolalpha >> b;
	return b;
}


bool prebuilt::by_name(const std::string& s)
{
	return get("prebuilt", g_prebuilt, s);
}

const std::string& versions::by_name(const std::string& s)
{
	return get("version", g_versions, s);
}

const fs::path& paths::by_name(const std::string& s)
{
	return get("path", g_paths, s);
}

void conf::set_log_level(int i)
{
	g_log = i;
}

bool conf::log_dump()         { return g_log > 5; }
bool conf::log_trace() 		  { return g_log > 4; }
bool conf::log_debug() 		  { return g_log > 3; }
bool conf::log_info() 		  { return g_log > 2; }
bool conf::log_warning()      { return g_log > 1; }
bool conf::log_error()        { return g_log > 0; }

template <class T>
bool parse_value(const std::string& s, T& out)
{
	out = s;
	return true;
}

template <>
bool parse_value<bool>(const std::string& s, bool& out)
{
	std::istringstream iss(s);
	iss >> std::boolalpha >> out;
	return !iss.bad();
}

template <class Map>
bool set_option_impl(
	Map& map, const std::string& key, const std::string& value)
{
	auto itor = map.find(key);
	if (itor == map.end())
	{
		gcx().error(context::conf, "unknown key '" + key + "'");
		return false;
	}

	if (!parse_value<typename Map::mapped_type>(value, itor->second))
	{
		gcx().error(context::conf, "bad value '" + value + "'");
		return false;
	}

	return true;
}

bool set_option(
	const std::string& section,
	const std::string& key, const std::string& value)
{
	if (section == "options")
		return set_option_impl(g_options, key, value);
	else if (section == "tools")
		return set_option_impl(g_tools, key, value);
	else if (section == "prebuilt")
		return set_option_impl(g_prebuilt, key, value);
	else if (section == "versions")
		return set_option_impl(g_versions, key, value);
	else if (section == "paths")
		return set_option_impl(g_paths, key, value);

	gcx().error(context::conf, "bad section name '" + section + "'");
	return false;
}

void set_option(const std::string& s)
{
	const auto slash = s.find("/");
	if (slash == std::string::npos)
	{
		gcx().bail_out(context::conf,
			"bad option " + s + ", must be section/key=value");
	}

	const auto equal = s.find("=", slash);
	if (slash == std::string::npos)
	{
		gcx().bail_out(context::conf,
			"bad option " + s + ", must be section/key=value");
	}

	const std::string section = s.substr(0, slash);
	const std::string key = s.substr(slash + 1, equal - slash - 1);
	const std::string value = s.substr(equal + 1);

	if (set_option(section, key, value))
	{
		gcx().trace(context::conf,
			"setting " + section + "/" + key + "=" + value);
	}
	else
	{
		gcx().bail_out(context::conf,
			"failed to set " + section + "/" + key + "=" + value);
	}
}

void dump_available_options()
{
	for (auto&& [k, v] : g_options)
		std::cout << "options/" << k << "\n";

	for (auto&& [k, v] : g_tools)
		std::cout << "tools/" << k << "\n";

	for (auto&& [k, v] : g_prebuilt)
		std::cout << "prebuilt/" << k << "\n";

	for (auto&& [k, v] : g_versions)
		std::cout << "versions/" << k << "\n";

	for (auto&& [k, v] : g_paths)
		std::cout << "paths/" << k << "\n";
}

bool try_parts(fs::path& check, const std::vector<std::string>& parts)
{
	for (std::size_t i=0; i<parts.size(); ++i)
	{
		fs::path p = check;

		for (std::size_t j=i; j<parts.size(); ++j)
			p /= parts[j];

		gcx().trace(context::conf, "trying parts " + p.string());

		if (fs::exists(p))
		{
			check = p;
			return true;
		}
	}

	return false;
}

fs::path find_root_impl()
{
	gcx().trace(context::conf, "looking for root directory");

	fs::path p = fs::current_path();

	if (try_parts(p, {"..", "..", "..", "third-party"}))
		return p;

	gcx().bail_out(context::conf, "root directory not found");
}

fs::path find_root()
{
	const auto p = find_root_impl().parent_path();

	gcx().trace(context::conf, "found root directory at " + p.string());

	return p;
}

fs::path find_in_root(const fs::path& file)
{
	static fs::path root = find_root();

	fs::path p = root / file;
	if (!fs::exists(p))
		gcx().bail_out(context::conf, p.string() + " not found");

	gcx().trace(context::conf, "found " + p.string());
	return p;
}


fs::path find_third_party_directory()
{
	static fs::path path = find_in_root("third-party");
	return path;
}


fs::path find_in_path(const std::string& exe)
{
	const std::size_t buffer_size = MAX_PATH;
	char buffer[buffer_size + 1] = {};

	if (SearchPathA(nullptr, exe.c_str(), nullptr, buffer_size, buffer, nullptr))
		return buffer;
	else
		return {};
}

bool find_qmake(fs::path& check)
{
	// try Qt/Qt5.14.2/msvc*/bin/qmake.exe
	if (try_parts(check, {
		"Qt",
		"Qt" + versions::qt(),
		"msvc" + versions::qt_vs() + "_64",
		"bin",
		"qmake.exe"}))
	{
		return true;
	}

	// try Qt/5.14.2/msvc*/bin/qmake.exe
	if (try_parts(check, {
		"Qt",
		versions::qt(),
		"msvc" + versions::qt_vs() + "_64",
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
	fs::path p = g_paths["qt_install"];

	if (!p.empty())
	{
		p = fs::absolute(p);

		if (!try_qt_location(p))
		{
			gcx().bail_out(context::conf,
				"no qt install in " + p.string());
		}

		return p;
	}


	std::vector<fs::path> locations =
	{
		paths::pf_x64(),
		"C:",
		"D:"
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
	fs::path p = paths::qt_install();

	if (!try_qt_location(p))
	{
		gcx().bail_out(context::conf,
			"qt path " + p.string() + " doesn't exist\n");
	}

	g_paths["qt_install"] = p;
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
			"failed to get x86 program files folder, defaulting to " +
			p.string(), e);
	}
	else
	{
		gcx().trace(context::conf,
			"x86 program files is " + p.string());
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
			"failed to get x64 program files folder, defaulting to " +
			p.string(), e);
	}
	else
	{
		gcx().trace(context::conf,
			"x64 program files is " + p.string());
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
		gcx().bail_out(context::conf, "can't get temp path", e);
	}

	fs::path p(buffer);
	gcx().trace(context::conf, "temp dir is " + p.string());

	return p;
}

fs::path find_vs()
{
	if (conf::dry())
		return tools::vswhere();

	auto p = process()
		.binary(tools::vswhere())
		.arg("-prerelease")
		.arg("-version", versions::vs())
		.arg("-property", "installationPath")
		.stdout_flags(process::keep_in_string)
		.stderr_flags(process::inherit);

	p.run();
	p.join();

	if (p.exit_code() != 0)
		gcx().bail_out(context::conf, "vswhere failed");

	fs::path path = trim_copy(p.steal_stdout());

	if (!fs::exists(path))
	{
		gcx().bail_out(context::conf,
			"the path given by vswhere doesn't exist: " + path.string());
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
	fs::path& bat = g_tools["vcvars"];

	if (conf::dry())
	{
		if (bat.empty())
			bat = "vcvars.bat";

		return;
	}


	if (bat.empty())
	{
		bat = paths::vs() / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat";

		if (!try_vcvars(bat))
			gcx().bail_out(context::conf, "vcvars not found " + bat.string());
	}
	else
	{
		if (!try_vcvars(bat))
			gcx().bail_out(context::conf, "vcvars not found " + bat.string());
	}

	gcx().trace(context::conf, "using vcvars at " + bat.string());
}


void ini_error(std::size_t line, const std::string& what)
{
	gcx().bail_out(context::conf,
		g_ini.filename().string() + ":" +
		std::to_string(line + 1) + ": " +
		what);
}

fs::path find_ini(const fs::path& ini)
{
	fs::path p = ini;

	if (!p.empty())
	{
		if (fs::exists(p))
			return fs::canonical(p);
		else
			gcx().bail_out(context::conf, "can't find ini at " + ini.string());
	}

	p = fs::current_path();

	if (try_parts(p, {"..", "..", "..", default_ini_filename}))
		return p;

	gcx().bail_out(context::conf, "can't find " + default_ini_filename);
}

std::vector<std::string> read_ini(const fs::path& ini)
{
	std::ifstream in(ini);

	std::vector<std::string> lines;

	for (;;)
	{
		std::string line;
		std::getline(in, line);
		trim(line);

		if (!in)
			break;

		if (line.empty() || line[0] == '#' || line[0] == ';')
			continue;

		lines.push_back(std::move(line));
	}

	if (in.bad())
		gcx().bail_out(context::conf, "failed to read ini " + ini.string());

	return lines;
}

void parse_section(
	std::size_t& i, const std::vector<std::string>& lines,
	const std::string& section)
{
	++i;

	for (;;)
	{
		if (i >= lines.size() || lines[i][0] == '[')
			break;

		const auto& line = lines[i];

		const auto sep = line.find("=");
		if (sep == std::string::npos)
			ini_error(i, "bad line '" + line + "'");

		const std::string k = trim_copy(line.substr(0, sep));
		const std::string v = trim_copy(line.substr(sep + 1));

		if (k.empty())
			ini_error(i, "bad line '" + line + "'");

		if (!set_option(section, k, v))
			ini_error(i, "bad line '" + line + "'");

		++i;
	}
}

void parse_ini(const fs::path& ini)
{
	g_ini = find_ini(ini);
	gcx().debug(context::conf, "using ini at " + g_ini.string());

	const auto lines = read_ini(g_ini);
	std::size_t i = 0;

	for (;;)
	{
		if (i >= lines.size())
			break;

		const auto& line = lines[i];

		if (line.starts_with("[") && line.ends_with("]"))
		{
			const std::string section = line.substr(1, line.size() - 2);
			parse_section(i, lines, section);
		}
		else
		{
			ini_error(i, "bad line '" + line + "'");
		}
	}
}

void check_missing_options()
{
	if (conf::mo_org().empty())
	{
		std::cerr
			<< "missing mo_org; either specify it the [options] section of "
			<< "the ini or pass '-s options/mo_org=something'\n";

		throw bailed("");
	}

	if (conf::mo_branch().empty())
	{
		std::cerr
			<< "missing mo_branch; either specify it the [options] section of "
			<< "the ini or pass '-s options/mo_org=something'\n";

		throw bailed("");
	}

	if (paths::prefix().empty())
	{
		std::cerr
			<< "missing prefix; either specify it the [paths] section of "
			<< "the ini or pass '-d path'\n";

		throw bailed("");
	}

	for (auto&& [k, v] : g_versions)
	{
		if (v.empty())
		{
			std::cerr << "missing version for " << k << "\n";
			throw bailed("");
		}
	}
}

template <class F>
void set_path_if_empty(const std::string& k, F&& f)
{
	auto itor = g_paths.find(k);
	if (itor == g_paths.end())
		gcx().bail_out(context::conf, "unknown path key " + k);

	if (!itor->second.empty())
	{
		if (conf::dry())
			return;

		if (fs::exists(itor->second))
		{
			itor->second = fs::canonical(itor->second);
			return;
		}
		else
		{
			gcx().bail_out(context::conf,
				"path " + itor->second.string() + " not found");
		}
	}

	fs::path cp;

	if constexpr (std::is_same_v<fs::path, std::decay_t<decltype(f)>>)
		cp = f;
	else
		cp = f();

	if (conf::dry())
	{
		itor->second = cp;
		return;
	}

	cp = fs::absolute(cp);

	if (!fs::exists(cp))
	{
		gcx().bail_out(context::conf,
			"path " + cp.string() + " not found");
	}

	itor->second = fs::canonical(cp);
}

void make_canonical_path(
	const std::string& key,
	const fs::path& default_parent, const std::string& default_dir)
{
	auto itor = g_paths.find(key);
	if (itor == g_paths.end())
		gcx().bail_out(context::conf, "unknown path key " + key);

	if (itor->second.empty())
	{
		itor->second = default_parent / default_dir;
	}
	else
	{
		if (itor->second.is_relative())
			itor->second = default_parent / itor->second;
	}

	if (!conf::dry())
		itor->second = fs::weakly_canonical(fs::absolute(itor->second));
}

void init_options(const fs::path& ini, const std::vector<std::string>& opts)
{
	parse_ini(ini);

	if (!opts.empty())
	{
		gcx().debug(context::conf, "overriding from command line:");

		for (auto&& o : opts)
			set_option(o);
	}

	check_missing_options();

	set_path_if_empty("third_party", find_third_party_directory);

	this_env::prepend_to_path(paths::third_party() / "bin");

	set_path_if_empty("vs",              find_vs);
	set_path_if_empty("qt_install",      find_qt);
	set_path_if_empty("pf_x86",          find_program_files_x86);
	set_path_if_empty("pf_x64",          find_program_files_x64);
	set_path_if_empty("temp_dir",        find_temp_dir);
	set_path_if_empty("patches",         find_in_root("patches"));
	set_path_if_empty("licenses",        find_in_root("licenses"));
	set_path_if_empty("qt_bin",          paths::qt_install() / "bin");

	find_vcvars();
	validate_qt();

	make_canonical_path("cache",            paths::prefix(), "downloads");
	make_canonical_path("build",            paths::prefix(), "build");
	make_canonical_path("install",          paths::prefix(), "install");
	make_canonical_path("install_bin",      paths::install(), "bin");
	make_canonical_path("install_libs",     paths::install(), "libs");
	make_canonical_path("install_pdbs",     paths::install(), "pdbs");
	make_canonical_path("install_dlls",     paths::install_bin(), "dlls");
	make_canonical_path("install_loot",     paths::install_bin(), "loot");
	make_canonical_path("install_plugins",  paths::install_bin(), "plugins");
	make_canonical_path("install_licenses", paths::install_bin(), "licenses");

	make_canonical_path(
		"install_pythoncore",
		paths::install_bin(), "pythoncore");

	make_canonical_path(
		"install_stylesheets",
		paths::install_bin(), "stylesheets");
}

template <class Map>
void table(const std::string& caption, const Map& values)
{
	std::size_t longest = 0;
	for (auto&& [k, v] : values)
		longest = std::max(longest, k.size());

	gcx().trace(context::conf, caption + ":");
	for (auto&& [k, v] : values)
		gcx().trace(context::conf, " . " + pad_right(k, longest) + " = " + v);
}

void dump_options()
{
	string_map opts = g_options;
	opts.insert({"log", std::to_string(g_log)});
	table("options", opts);

	string_map tools;
	for (auto&& [k, v] : g_tools)
		tools[k] = v.string();
	table("tools", tools);

	string_map prebuilt;
	for (auto&& [k, v] : g_prebuilt)
		prebuilt[k] = (v ? "true" : "false");
	table("prebuilt", prebuilt);

	table("versions", g_versions);

	string_map paths;
	for (auto&& [k, v] : g_paths)
		paths[k] = v.string();
	table("paths", paths);
}


fs::path make_temp_file()
{
	static fs::path dir = paths::temp_dir();

	wchar_t name[MAX_PATH + 1] = {};
	if (GetTempFileNameW(dir.native().c_str(), L"mob", 0, name) == 0)
	{
		const auto e = GetLastError();

		gcx().bail_out(context::conf,
			"can't create temp file in " + dir.string(), e);
	}

	return dir / name;
}

}	// namespace

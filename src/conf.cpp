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
const std::string ini_filename = "mob.ini";

// special case for options that are used often to avoid string manipulations
static bool g_dry = false;
static int  g_log = 3;
static bool g_redownload = false;
static bool g_reextract = false;
static bool g_rebuild = false;

static string_map g_options =
{
	{"mo_org",    "ModOrganizer2"},
	{"mo_branch", "master"}
};

static path_map g_third_party =
{
	{"sevenz",  ""},
	{"jom",     ""},
	{"patch",   ""},
	{"nuget",   ""},
	{"vswhere", ""},
};

static bool_map g_prebuilt =
{
	{"boost",   false}
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
};

static path_map g_paths =
{
	{"third_party" ,    ""},
	{"prefix",          ""},
	{"cache",           ""},
	{"build",           ""},
	{"install",         ""},
	{"install_bin",     ""},
	{"install_libs",    ""},
	{"install_pdbs",    ""},
	{"install_dlls",    ""},
	{"install_loot",    ""},
	{"install_plugins", ""},
	{"patches",         ""},
	{"vs",              ""},
	{"vcvars",          ""},
	{"qt_install",      ""},
	{"qt_bin",          ""},
	{"pf_x86",          ""},
	{"pf_x64",          ""},
	{"temp_dir",        ""},
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


const std::string& conf::by_name(const std::string& name)
{
	return get("option", g_options, name);
}

const fs::path& third_party::by_name(const std::string& s)
{
	return get("third party", g_third_party, s);
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


bool conf::log_dump()         { return g_log > 5; }
bool conf::log_trace() 		  { return g_log > 4; }
bool conf::log_debug() 		  { return g_log > 3; }
bool conf::log_info() 		  { return g_log > 2; }
bool conf::log_warning()      { return g_log > 1; }
bool conf::log_error()        { return g_log > 0; }
bool conf::dry()              { return g_dry; }
bool conf::redownload()       { return g_redownload; }
bool conf::reextract()        { return g_reextract; }
bool conf::rebuild()          { return g_rebuild; }


fs::path tool_paths::perl()    { return "perl.exe"; }
fs::path tool_paths::msbuild() { return "msbuild.exe"; }
fs::path tool_paths::devenv()  { return "devenv.exe"; }
fs::path tool_paths::cmake()   { return "cmake.exe"; }
fs::path tool_paths::git()     { return "git.exe"; }


void conf_command_line_options(clipp::group& g)
{
	g.push_back(
		(clipp::option("-i", "--ini")
			>> [&](auto&& v){ g_ini = v; })
			% ("path to the ini file (defaults to ./" + ini_filename + ")"),

		(clipp::option("--dry")
			>> [&]{ g_dry = true; })
			%  "simulates filesystem operations",

		(clipp::option("-l", "--log-level")
			&  clipp::value("level", g_log))
			%  "0 is silent, 6 is max",

		//(clipp::option("-o", "--option")
		//	&  clipp::value("key=value")
		//	>> [&](auto&& v){ set_opt(v); })
		//	%  "sets an option",

		(clipp::option("-g", "--redownload")
			>> [&]{ g_redownload = true; })
			% "redownloads archives, see --reextract",

		(clipp::option("-e", "--reextract")
			>> [&]{ g_reextract = true; })
			% "deletes source directories and re-extracts archives",

		(clipp::option("-b", "--rebuild")
			>> [&]{ g_rebuild = true; })
			%  "cleans and rebuilds projects",

		(clipp::option("-c", "--clean")
			>> [&]{ g_redownload=true; g_reextract=true; g_rebuild=true; })
			% "combines --redownload, --reextract and --rebuild"
	);
}


bool try_parts(fs::path& check, const std::vector<std::string>& parts)
{
	for (std::size_t i=0; i<parts.size() - 1; ++i)
	{
		fs::path p = check;

		for (std::size_t j=i; j<parts.size(); ++j)
			p /= parts[j];

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
	auto p = process()
		.binary(third_party::vswhere())
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

fs::path find_vcvars()
{
	const auto vcvars =
		paths::vs() / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat";

	if (!fs::exists(vcvars))
	{
		gcx().bail_out(context::conf,
			"can't find vcvars batch file at " + vcvars.string());
	}

	return vcvars;
}



template <class T>
bool parse_value(const std::string& s, T& out) = delete;

template <>
bool parse_value<bool>(const std::string& s, bool& out)
{
	std::istringstream iss(s);
	iss >> out;
	return !iss.bad();
}

template <>
bool parse_value<std::string>(const std::string& s, std::string& out)
{
	out = s;
	return true;
}

template <>
bool parse_value<fs::path>(const std::string& s, fs::path& out)
{
	out = s;
	return true;
}


void ini_error(std::size_t line, const std::string& what)
{
	gcx().bail_out(context::conf,
		g_ini.filename().string() + ":" +
		std::to_string(line + 1) + ": " +
		what);
}

fs::path find_ini()
{
	fs::path p = g_ini;

	if (!p.empty())
	{
		if (fs::exists(p))
			return fs::canonical(p);
	}

	p = fs::current_path();

	if (try_parts(p, {"..", "..", "..", ini_filename}))
		return p;

	gcx().bail_out(context::conf, "can't find " + ini_filename);
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

template <class Map>
void parse_section(
	std::size_t& i, const std::vector<std::string>& lines, Map& map)
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

		auto itor = map.find(k);
		if (itor == map.end())
			ini_error(i, "unknown key '" + k + "'");

		if (!parse_value<typename Map::mapped_type>(v, itor->second))
			ini_error(i, "bad line '" + line + "'");

		++i;
	}
}

void parse_ini()
{
	g_ini = find_ini();
	gcx().debug(context::conf, "using ini at " + g_ini.string());

	const auto lines = read_ini(g_ini);
	std::size_t i = 0;

	for (;;)
	{
		if (i >= lines.size())
			break;

		if (lines[i] == "[options]")
			parse_section(i, lines, g_options);
		else if (lines[i] == "[third-party]")
			parse_section(i, lines, g_third_party);
		else if (lines[i] == "[prebuilt]")
			parse_section(i, lines, g_prebuilt);
		else if (lines[i] == "[versions]")
			parse_section(i, lines, g_versions);
		else if (lines[i] == "[paths]")
			parse_section(i, lines, g_paths);
		else
			ini_error(i, "bad section " + lines[i]);
	}
}

void check_missing_options()
{
	if (conf::mo_org().empty())
		gcx().bail_out(context::conf, "missing mo_org in [options]");

	if (conf::mo_branch().empty())
		gcx().bail_out(context::conf, "missing mo_branch in [options]");

	if (paths::prefix().empty())
		gcx().bail_out(context::conf, "missing prefix in [paths]");

	for (auto&& [k, v] : g_versions)
	{
		if (v.empty())
			gcx().bail_out(context::conf, "missing version for " + k);
	}
}


void find_in_third_party(const std::string& k, const std::string& exe)
{
	auto itor = g_third_party.find(k);

	if (itor == g_third_party.end())
		gcx().bail_out(context::conf, "unknown third party key " + k);

	if (!itor->second.empty())
	{
		if (fs::exists(itor->second))
		{
			itor->second = fs::canonical(itor->second);
			return;
		}
		else
		{
			gcx().bail_out(context::conf,
				"third party " + itor->second.string() + " not found");
		}
	}

	auto p = paths::third_party() / "bin" / exe;

	if (fs::exists(p))
	{
		itor->second  = fs::canonical(p);
		return;
	}

	p = find_in_path(exe);
	if (fs::exists(p))
	{
		itor->second = fs::canonical(p);
		return;
	}

	gcx().bail_out(context::conf, "can't find " + exe);
}

template <class F>
void set_path_if_empty(const std::string& k, F&& f)
{
	auto itor = g_paths.find(k);
	if (itor == g_paths.end())
		gcx().bail_out(context::conf, "unknown path party key " + k);

	if (!itor->second.empty())
	{
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
		cp = fs::absolute(f);
	else
		cp = fs::absolute(f());

	if (!fs::exists(cp))
	{
		gcx().bail_out(context::conf,
			"path " + cp.string() + " not found");
	}

	itor->second = fs::canonical(cp);
}


void init_options()
{
	parse_ini();

	check_missing_options();

	set_path_if_empty("third_party",      find_third_party_directory);

	find_in_third_party("sevenz",        "7z.exe");
	find_in_third_party("jom",           "jom.exe");
	find_in_third_party("patch",         "patch.exe");
	find_in_third_party("vswhere",       "vswhere.exe");
	find_in_third_party("nuget",         "nuget.exe");

	set_path_if_empty("vs",              find_vs);
	set_path_if_empty("vcvars",          find_vcvars);
	set_path_if_empty("qt_install",      find_qt);
	set_path_if_empty("pf_x86",          find_program_files_x86);
	set_path_if_empty("pf_x64",          find_program_files_x64);
	set_path_if_empty("temp_dir",        find_temp_dir);
	set_path_if_empty("patches",         []{ return find_in_root("patches"); });

	set_path_if_empty("cache",           paths::prefix() / "downloads");
	set_path_if_empty("build",           paths::prefix() / "build");
	set_path_if_empty("install",         paths::prefix() / "install");
	set_path_if_empty("install_bin",     paths::install() / "bin");
	set_path_if_empty("install_libs",    paths::install() / "libs");
	set_path_if_empty("install_pdbs",    paths::install() / "pdbs");
	set_path_if_empty("install_dlls",    paths::install_bin() / "dlls");
	set_path_if_empty("install_loot",    paths::install_bin() / "loot");
	set_path_if_empty("install_plugins", paths::install_bin() / "plugins");
	set_path_if_empty("qt_bin",          paths::qt_install() / "bin");

	validate_qt();
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
	const string_map manual_opts =
	{
		{"dry",         g_dry ? "true" : "false"},
		{"log",         std::to_string(g_log)},
		{"redownload",  g_redownload ? "true" : "false"},
		{"reextract",   g_reextract ? "true" : "false"},
		{"rebuild",     g_rebuild ? "true" : "false"},
	};

	string_map opts = g_options;
	opts.insert(manual_opts.begin(),  manual_opts.end());
	table("options", g_options);

	string_map third_party;
	for (auto&& [k, v] : g_third_party)
		third_party[k] = v.string();
	table("third-party", third_party);

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

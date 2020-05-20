#include "pch.h"
#include "conf.h"
#include "utility.h"
#include "context.h"
#include "process.h"
#include "tasks/task.h"
#include "tools/tools.h"

namespace mob
{

conf::task_map conf::map_;
int conf::output_log_level_ = 3;
int conf::file_log_level_ = 5;
bool conf::dry_ = false;

std::string master_ini_filename()
{
	return "mob.ini";
}


std::string conf::get_global(const std::string& section, const std::string& key)
{
	auto global = map_.find("");
	MOB_ASSERT(global != map_.end());

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

void conf::set_global(
	const std::string& section,
	const std::string& key, const std::string& value)
{
	auto global = map_.find("");
	MOB_ASSERT(global != map_.end());

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

void conf::add_global(
	const std::string& section,
	const std::string& key, const std::string& value)
{
	map_[""][section][key] = value;
}

std::string conf::get_for_task(
	const std::vector<std::string>& task_names,
	const std::string& section, const std::string& key)
{
	task_map::iterator task = map_.end();

	for (auto&& tn : task_names)
	{
		task = map_.find(tn);
		if (task != map_.end())
			break;
	}

	if (task == map_.end())
	{
		for (auto&& tn : task_names)
		{
			if (is_super_task(tn))
			{
				task = map_.find("super");
				break;
			}
		}
	}

	if (task == map_.end())
		return get_global(section, key);

	auto sitor = task->second.find(section);
	if (sitor == task->second.end())
		return get_global(section, key);

	auto kitor = sitor->second.find(key);
	if (kitor == sitor->second.end())
		return get_global(section, key);

	return kitor->second;
}

void conf::set_for_task(
	const std::string& task_name, const std::string& section,
	const std::string& key, const std::string& value)
{
	// make sure the key exists, will throw if it doesn't
	get_global(section, key);

	map_[task_name][section][key] = value;
}

bool conf::prebuilt_by_name(const std::string& task)
{
	const std::string s = get_global("prebuilt", task);
	return (s == "true" || s == "yes" || s == "1");
}

fs::path conf::path_by_name(const std::string& name)
{
	return get_global("paths", name);
}

std::string conf::version_by_name(const std::string& name)
{
	return get_global("versions", name);
}

fs::path conf::tool_by_name(const std::string& name)
{
	return get_global("tools", name);
}

std::string conf::global_by_name(const std::string& name)
{
	return get_global("global", name);
}

bool conf::bool_global_by_name(const std::string& name)
{
	const std::string s = global_by_name(name);
	return (s == "true" || s == "yes" || s == "1");
}

std::string conf::option_by_name(
	const std::vector<std::string>& task_names, const std::string& name)
{
	return get_for_task(task_names, "options", name);
}

bool conf::bool_option_by_name(
	const std::vector<std::string>& task_names, const std::string& name)
{
	const std::string s = option_by_name(task_names, name);
	return (s == "true" || s == "yes" || s == "1");
}

void conf::set_output_log_level(const std::string& s)
{
	if (s.empty())
		return;

	try
	{
		const auto i = std::stoi(s);

		if (i < 0 || i > 6)
			gcx().bail_out(context::generic, "bad output log level {}", i);

		output_log_level_ = i;
	}
	catch(std::exception&)
	{
		gcx().bail_out(context::generic, "bad output log level {}", s);
	}
}

void conf::set_file_log_level(const std::string& s)
{
	if (s.empty())
		return;

	try
	{
		const auto i = std::stoi(s);
		if (i < 0 || i > 6)
			gcx().bail_out(context::generic, "bad file log level {}", i);

		file_log_level_ = i;
	}
	catch(std::exception&)
	{
		gcx().bail_out(context::generic, "bad file log level {}", s);
	}
}

void conf::set_dry(const std::string& s)
{
	dry_ = (s == "true" || s == "yes" || s == "1");
}


std::vector<std::string> conf::format_options()
{
	std::size_t longest_task = 0;
	std::size_t longest_section = 0;
	std::size_t longest_key = 0;

	for (auto&& [t, ss] : map_)
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

	for (auto&& [t, ss] : map_)
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


struct parsed_option
{
	std::string task, section, key, value;
};

parsed_option parse_option(const std::string& s)
{
	// task:section/key=value
	// task: is optional
	std::regex re(R"((?:(.+)\:)?(.+)/(.*)=(.*))");
	std::smatch m;

	if (!std::regex_match(s, m, re))
	{
		gcx().bail_out(context::conf,
			"bad option {}, must be [task:]section/key=value", s);
	}

	std::string task = trim_copy(m[1].str());
	std::string section = trim_copy(m[2].str());
	std::string key = trim_copy(m[3].str());
	std::string value = trim_copy(m[4].str());

	return {task, section, key, value};
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

fs::path find_root()
{
	gcx().trace(context::conf, "looking for root directory");

	fs::path p = mob_exe_path().parent_path();

	if (try_parts(p, {"..", "..", "..", "third-party"}))
	{
		p = fs::canonical(p.parent_path());
		gcx().trace(context::conf, "found root directory at {}", p);
		return p;
	}

	gcx().bail_out(context::conf, "root directory not found");
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


fs::path find_in_path(const std::string& exe)
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
	fs::path p = conf::path_by_name("qt_install");

	if (!p.empty())
	{
		p = fs::absolute(p);

		if (!try_qt_location(p))
			gcx().bail_out(context::conf, "no qt install in {}", p);

		return p;
	}


	std::vector<fs::path> locations =
	{
		paths::pf_x64(),
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

	conf::set_global("paths", "qt_install", path_to_utf8(p));
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

	auto p = process()
		.binary(vs::vswhere())
		.arg("-prerelease")
		.arg("-version", vs::version())
		.arg("-property", "installationPath")
		.stdout_flags(process::keep_in_string)
		.stderr_flags(process::inherit);

	p.run();
	p.join();

	if (p.exit_code() != 0)
		gcx().bail_out(context::conf, "vswhere failed");

	fs::path path = trim_copy(p.stdout_string());

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
	fs::path bat = conf::tool_by_name("vcvars");

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

	conf::set_global("tools", "vcvars", path_to_utf8(bat));
	gcx().trace(context::conf, "using vcvars at {}", bat);
}


void ini_error(const fs::path& ini, std::size_t line, const std::string& what)
{
	gcx().bail_out(context::conf,
		"{}:{}: {}",
		ini.filename(), (line + 1), what);
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
		gcx().bail_out(context::conf, "failed to read ini {}", ini);

	return lines;
}

void parse_section(
	const fs::path& ini, std::size_t& i,
	const std::vector<std::string>& lines,
	const std::string& task, const std::string& section, bool add)
{
	++i;

	for (;;)
	{
		if (i >= lines.size() || lines[i][0] == '[')
			break;

		const auto& line = lines[i];

		const auto sep = line.find("=");
		if (sep == std::string::npos)
			ini_error(ini, i, "bad line '" + line + "'");

		const std::string k = trim_copy(line.substr(0, sep));
		const std::string v = trim_copy(line.substr(sep + 1));

		if (k.empty())
			ini_error(ini, i, "bad line '" + line + "'");

		if (task.empty())
		{
			if (add)
				conf::add_global(section, k, v);
			else
				conf::set_global(section, k, v);
		}
		else
		{
			if (!task_exists(task))
				ini_error(ini, i, "task '" + task + "' doesn't exist");

			conf::set_for_task(task, section, k, v);
		}

		++i;
	}
}

void parse_ini(const fs::path& ini, bool add)
{
	gcx().debug(context::conf, "using ini at {}", ini);

	const auto lines = read_ini(ini);
	std::size_t i = 0;

	for (;;)
	{
		if (i >= lines.size())
			break;

		const auto& line = lines[i];

		if (line.starts_with("[") && line.ends_with("]"))
		{
			const std::string s = line.substr(1, line.size() - 2);

			std::string task, section;

			const auto col = s.find(":");

			if (col == std::string::npos)
			{
				section = s;
			}
			else
			{
				task = s.substr(0, col);
				section = s.substr(col + 1);
			}

			parse_section(ini, i, lines, task, section, add);
		}
		else
		{
			ini_error(ini, i, "bad line '" + line + "'");
		}
	}
}

bool check_missing_options()
{
	if (paths::prefix().empty())
	{
		u8cerr
			<< "missing prefix; either specify it the [paths] section of "
			<< "the ini or pass '-d path'\n";

		return false;
	}

	return true;
}

template <class F>
void set_path_if_empty(const std::string& k, F&& f)
{
	fs::path p = conf::get_global("paths", k);

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

	conf::set_global("paths", k, path_to_utf8(p));
}

void make_canonical_path(
	const std::string& key,
	const fs::path& default_parent, const std::string& default_dir)
{
	fs::path p = conf::path_by_name(key);

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

	conf::set_global("paths", key, path_to_utf8(p));
}

void set_special_options()
{
	conf::set_output_log_level(conf::get_global("global", "output_log_level"));
	conf::set_file_log_level(conf::get_global("global", "file_log_level"));
	conf::set_dry(conf::get_global("global", "dry"));
}

std::vector<fs::path> find_inis(
	bool auto_detect, const std::vector<std::string>& from_cl, bool verbose)
{
	std::vector<fs::path> v;

	auto add_or_move_up = [&](fs::path p)
	{
		for (auto itor=v.begin(); itor!=v.end(); ++itor)
		{
			if (fs::equivalent(p, *itor))
			{
				v.erase(itor);
				v.push_back(p);
				return;
			}
		}

		v.push_back(p);
	};


	// auto detect from exe directory and cwd
	if (auto_detect)
	{
		if (verbose)
			u8cout << "root is " << path_to_utf8(find_root()) << "\n";

		const auto master = find_in_root(master_ini_filename());

		if (verbose)
			u8cout << "found master " << master_ini_filename() << "\n";

		v.push_back(master);

		const auto in_cwd = fs::current_path() / master_ini_filename();
		if (fs::exists(in_cwd) && !fs::equivalent(in_cwd, master))
		{
			if (verbose)
				u8cout << "also found in cwd " << path_to_utf8(in_cwd) << "\n";

			v.push_back(fs::canonical(in_cwd));
		}
	}

	// MOBINI environment variable
	if (auto e=this_env::get_opt("MOBINI"))
	{
		if (verbose)
			u8cout << "found env MOBINI: " << *e << "\n";

		for (auto&& i : split(*e, ";"))
		{
			if (verbose)
				u8cout << "checking '" << i << "'\n";

			auto p = fs::path(i);
			if (!fs::exists(p))
			{
				u8cerr << "ini from env MOBINI " << i << " not found\n";
				throw bailed();
			}

			p = fs::canonical(p);

			if (verbose)
				u8cout << "ini from env: " << path_to_utf8(p) << "\n";

			add_or_move_up(p);
		}
	}

	for (auto&& i : from_cl)
	{
		auto p = fs::path(i);
		if (!fs::exists(p))
		{
			u8cerr << "ini " << i << " not found\n";
			throw bailed();
		}

		p = fs::canonical(p);

		if (verbose)
			u8cout << "ini from command line: " << path_to_utf8(p) << "\n";

		add_or_move_up(p);
	}

	return v;
}

void init_options(
	const std::vector<fs::path>& inis, const std::vector<std::string>& opts)
{
	MOB_ASSERT(!inis.empty());

	bool add = true;
	for (auto&& ini : inis)
	{
		parse_ini(ini, add);
		add = false;
	}

	if (!opts.empty())
	{
		gcx().debug(context::conf, "overriding from command line:");

		for (auto&& o : opts)
		{
			const auto po = parse_option(o);

			if (po.task.empty())
			{
				conf::set_global(po.section, po.key, po.value);
			}
			else
			{
				if (!task_exists(po.task))
				{
					gcx().bail_out(context::generic,
						"task '{}' doesn't exist (command line option)",
						po.task);
				}

				conf::set_for_task(po.task, po.section, po.key, po.value);
			}
		}
	}

	set_special_options();
	context::set_log_file(conf::log_file());

	gcx().debug(context::conf,
		"command line: {}", std::wstring(GetCommandLineW()));

	gcx().debug(context::conf, "using inis in order:");

	for (auto&& ini : inis)
		gcx().debug(context::conf, "  . {}", ini);

	set_path_if_empty("third_party", find_third_party_directory);
	this_env::prepend_to_path(paths::third_party() / "bin");

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

	if (!paths::prefix().empty())
		make_canonical_path("prefix",           fs::current_path(), "");

	make_canonical_path("cache",            paths::prefix(), "downloads");
	make_canonical_path("build",            paths::prefix(), "build");
	make_canonical_path("install",          paths::prefix(), "install");
	make_canonical_path("install_bin",      paths::install(), "bin");
	make_canonical_path("install_libs",     paths::install(), "libs");
	make_canonical_path("install_pdbs",     paths::install(), "pdb");
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

bool verify_options()
{
	return check_missing_options();
}

void log_options()
{
	for (auto&& line : conf::format_options())
		gcx().trace(context::conf, "{}", line);
}

void dump_available_options()
{
	for (auto&& line : conf::format_options())
		u8cout << line << "\n";
}


fs::path make_temp_file()
{
	static fs::path dir = paths::temp_dir();

	wchar_t name[MAX_PATH + 1] = {};
	if (GetTempFileNameW(dir.native().c_str(), L"mob", 0, name) == 0)
	{
		const auto e = GetLastError();

		gcx().bail_out(context::conf,
			"can't create temp file in {}, {}", dir, error_message(e));
	}

	return dir / name;
}

}	// namespace

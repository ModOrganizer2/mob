#pragma once

namespace mob
{

class conf_global;
class conf_prebuilt;
class conf_transifex;
class conf_paths;

class conf
{
public:
	conf_global global();
	conf_transifex transifex();
	conf_prebuilt prebuilt();
	conf_paths paths();


	static std::string task_option_by_name(
		const std::vector<std::string>& task_names, std::string_view name);

	static bool bool_task_option_by_name(
		const std::vector<std::string>& task_names, std::string_view name);

	static fs::path path_by_name(std::string_view name);
	static std::string version_by_name(std::string_view name);
	static fs::path tool_by_name(std::string_view name);

	static int output_log_level() { return output_log_level_; }
	static void set_output_log_level(const std::string& s);

	static int file_log_level()   { return file_log_level_; }
	static void set_file_log_level(const std::string& s);

	static bool dry() { return dry_; }
	static void set_dry(std::string_view s);

	static fs::path log_file() { return global_by_name("log_file"); }
	static bool redownload()   { return bool_global_by_name("redownload"); }
	static bool reextract()    { return bool_global_by_name("reextract"); }
	static bool reconfigure()  { return bool_global_by_name("reconfigure"); }
	static bool rebuild()      { return bool_global_by_name("rebuild"); }
	static bool clean()        { return bool_global_by_name("clean_task"); }
	static bool fetch()        { return bool_global_by_name("fetch_task"); }
	static bool build()        { return bool_global_by_name("build_task"); }

	static bool ignore_uncommitted()
	{
		return bool_global_by_name("ignore_uncommitted");
	}

	static std::vector<std::string> format_options();

	// only in conf.cpp

	static std::string get_global(
		std::string_view section, std::string_view key);

	static int get_global_int(
		std::string_view section, std::string_view key);

	static bool get_global_bool(
		std::string_view section, std::string_view key);

	static void add_global(
		std::string_view section,
		std::string_view key, std::string_view value);

	static std::string get_for_task(
		const std::vector<std::string>& task_names,
		std::string_view section, std::string_view key);

	static void set_for_task(
		std::string_view task_name, std::string_view section,
		std::string_view key, std::string_view value);

	static void set_global(
		std::string_view section,
		std::string_view key, std::string_view value);

	static std::string global_by_name(std::string_view name);
	static bool bool_global_by_name(std::string_view name);

private:
	using key_value_map = std::map<std::string, std::string, std::less<>>;
	using section_map = std::map<std::string, key_value_map, std::less<>>;
	using task_map = std::map<std::string, section_map, std::less<>>;

	static task_map map_;

	// special cases to avoid string manipulations
	static int output_log_level_;
	static int file_log_level_;
	static bool dry_;

	static std::optional<std::string> find_for_task(
		std::string_view task_name,
		std::string_view section, std::string_view key);
};


std::string default_ini_filename();

std::vector<fs::path> find_inis(
	bool auto_detect, const std::vector<std::string>& from_cl,
	bool verbose);

void init_options(
	const std::vector<fs::path>& inis, const std::vector<std::string>& opts);

bool verify_options();
void log_options();
void dump_available_options();

fs::path find_in_path(std::string_view exe);
fs::path make_temp_file();



class conf_section
{
public:
	std::string get(std::string_view key)
	{
		return conf::get_global(name_, key);
	}

	template <class T>
	T get(std::string_view key);

	template <>
	bool get<bool>(std::string_view key)
	{
		return conf::get_global_bool(name_, key);
	}

	template <>
	int get<int>(std::string_view key)
	{
		return conf::get_global_int(name_, key);
	}

	void set(std::string_view key, std::string_view value)
	{
		conf::set_global(name_, key, value);
	}

protected:
	conf_section(std::string section_name)
		: name_(std::move(section_name))
	{
	}

private:
	std::string name_;
};


class conf_global : public conf_section
{
public:
	conf_global();
};

class conf_transifex : public conf_section
{
public:
	conf_transifex();
};

class conf_prebuilt : public conf_section
{
public:
	conf_prebuilt();

	bool use_prebuilt(std::string_view task_name)
	{
		return get<bool>(task_name);
	}
};


class conf_paths : public conf_section
{
public:
	conf_paths();

#define VALUE(NAME) \
	static fs::path NAME() { return conf::path_by_name(#NAME); }

	VALUE(third_party);
	VALUE(prefix);
	VALUE(cache);
	VALUE(patches);
	VALUE(licenses);
	VALUE(build);

	VALUE(install);
	VALUE(install_installer);
	VALUE(install_bin);
	VALUE(install_libs);
	VALUE(install_pdbs);

	VALUE(install_dlls);
	VALUE(install_loot);
	VALUE(install_plugins);
	VALUE(install_stylesheets);
	VALUE(install_licenses);
	VALUE(install_pythoncore);
	VALUE(install_translations);

	VALUE(pf_x86);
	VALUE(pf_x64);
	VALUE(temp_dir);

#undef VALUE
};

}	// namespace

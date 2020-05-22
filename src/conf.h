#pragma once

namespace mob
{

class conf
{
public:
	static std::string get_global(
		const std::string& section, const std::string& key);

	static void set_global(
		const std::string& section,
		const std::string& key, const std::string& value);

	static void add_global(
		const std::string& section,
		const std::string& key, const std::string& value);


	static std::string get_for_task(
		const std::vector<std::string>& task_names,
		const std::string& section, const std::string& key);

	static void set_for_task(
		const std::string& task_name, const std::string& section,
		const std::string& key, const std::string& value);


	static bool prebuilt_by_name(const std::string& task);
	static fs::path path_by_name(const std::string& name);
	static std::string version_by_name(const std::string& name);
	static fs::path tool_by_name(const std::string& name);

	static std::string global_by_name(const std::string& name);
	static bool bool_global_by_name(const std::string& name);

	static std::string option_by_name(
		const std::vector<std::string>& task_names, const std::string& name);

	static bool bool_option_by_name(
		const std::vector<std::string>& task_names, const std::string& name);


	static int output_log_level() { return output_log_level_; }
	static void set_output_log_level(const std::string& s);

	static int file_log_level()   { return file_log_level_; }
	static void set_file_log_level(const std::string& s);

	static bool dry() { return dry_; }
	static void set_dry(const std::string& s);

	static fs::path log_file() { return global_by_name("log_file"); }
	static bool redownload()   { return bool_global_by_name("redownload"); }
	static bool reextract()    { return bool_global_by_name("reextract"); }
	static bool reconfigure()  { return bool_global_by_name("reconfigure"); }
	static bool rebuild()      { return bool_global_by_name("rebuild"); }

	static std::vector<std::string> format_options();

private:
	using key_value_map = std::map<std::string, std::string>;
	using section_map = std::map<std::string, key_value_map>;
	using task_map = std::map<std::string, section_map>;

	static task_map map_;

	// special cases to avoid string manipulations
	static int output_log_level_;
	static int file_log_level_;
	static bool dry_;

	static std::optional<std::string> find_for_task(
		const std::string& task_name,
		const std::string& section, const std::string& key);
};


struct paths
{
#define VALUE(NAME) \
	static fs::path NAME() { return conf::path_by_name(#NAME); }

	VALUE(third_party);
	VALUE(prefix);
	VALUE(cache);
	VALUE(patches);
	VALUE(licenses);
	VALUE(build);

	VALUE(install);
	VALUE(install_bin);
	VALUE(install_libs);
	VALUE(install_pdbs);

	VALUE(install_dlls);
	VALUE(install_loot);
	VALUE(install_plugins);
	VALUE(install_stylesheets);
	VALUE(install_licenses);
	VALUE(install_pythoncore);

	VALUE(pf_x86);
	VALUE(pf_x64);
	VALUE(temp_dir);

#undef VALUE
};


std::string master_ini_filename();

std::vector<fs::path> find_inis(
	bool auto_detect, const std::vector<std::string>& from_cl,
	bool verbose);

void init_options(
	const std::vector<fs::path>& inis, const std::vector<std::string>& opts);

bool verify_options();
void log_options();
void dump_available_options();

fs::path find_in_path(const std::string& exe);
fs::path make_temp_file();

}	// namespace

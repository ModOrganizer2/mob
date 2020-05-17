#pragma once

namespace mob
{

bool prebuilt_by_name(const std::string& task);
std::string version_by_name(const std::string& s);
fs::path tool_by_name(const std::string& s);
fs::path path_by_name(const std::string& s);
std::string conf_by_name(const std::string& s);
bool bool_conf_by_name(const std::string& s);

struct conf
{
	static int output_log_level();
	static int file_log_level();

	static std::string mo_org()    { return conf_by_name("mo_org"); }
	static std::string mo_branch() { return conf_by_name("mo_branch"); }
	static fs::path log_file()     { return conf_by_name("log_file"); }

	static bool dry()        { return bool_conf_by_name("dry"); }
	static bool redownload() { return bool_conf_by_name("redownload"); }
	static bool reextract()  { return bool_conf_by_name("reextract"); }
	static bool rebuild()    { return bool_conf_by_name("rebuild"); }
};

struct paths
{
#define VALUE(NAME) \
	static fs::path NAME() { return path_by_name(#NAME); }

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


void init_options(const fs::path& ini, const std::vector<std::string>& opts);
bool verify_options();
void log_options();
void dump_available_options();

fs::path make_temp_file();

}	// namespace

#pragma once

namespace mob
{

#define VALUE(NAME) \
	static decltype(auto) NAME() { return by_name(#NAME); }

#define VALUE_BOOL(NAME) \
	static bool NAME() { return by_name_bool(#NAME); }

struct conf
{
	static void set_output_log_level(int i);
	static void set_file_log_level(int i);
	static void set_log_file(const fs::path& p);

	static int output_log_level();
	static int file_log_level();
	static const fs::path& log_file();

	static const std::string& by_name(const std::string& s);
	static bool by_name_bool(const std::string& name);

	VALUE(mo_org);
	VALUE(mo_branch);

	VALUE_BOOL(dry);
	VALUE_BOOL(redownload);
	VALUE_BOOL(reextract);
	VALUE_BOOL(rebuild);
};

struct paths
{
	static const fs::path& by_name(const std::string& s);

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
};

#undef VALUE_BOOL
#undef VALUE


bool prebuilt_by_name(const std::string& task);
const std::string& version_by_name(const std::string& s);
const fs::path& tool_by_name(const std::string& s);

void init_options(const fs::path& ini, const std::vector<std::string>& opts);
bool verify_options();
void dump_options();
void dump_available_options();

fs::path make_temp_file();

}	// namespace

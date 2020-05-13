#pragma once

namespace mob
{

#define VALUE(NAME) \
	static decltype(auto) NAME() { return by_name(#NAME); }

#define VALUE_BOOL(NAME) \
	static bool NAME() { return by_name_bool(#NAME); }


namespace tools
{
	struct perl
	{
		static fs::path binary();
	};

	struct msbuild
	{
		static fs::path binary();
	};

	struct devenv
	{
		static fs::path binary();
	};

	struct cmake
	{
		static fs::path binary();
	};

	struct git
	{
		static fs::path binary();
	};

	struct sevenz
	{
		static fs::path binary();
	};

	struct jom
	{
		static fs::path binary();
	};

	struct nasm
	{
		static fs::path binary();
	};

	struct patch
	{
		static fs::path binary();
	};

	struct nuget
	{
		static fs::path binary();
	};

	struct vs
	{
		static fs::path installation_path();
		static fs::path vswhere();
		static fs::path vcvars();
		static std::string version();
		static std::string year();
		static std::string toolset();
		static std::string sdk();
	};

	struct qt
	{
		static fs::path installation_path();
		static fs::path bin_path();
		static std::string version();
		static std::string vs_version();
	};
};


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

struct prebuilt
{
	static bool by_name(const std::string& s);
};

struct versions
{
	static const std::string& by_name(const std::string& s);

	VALUE(ss_6788_paper_lad);
	VALUE(ss_6788_paper_automata);
	VALUE(ss_6788_paper_mono);
	VALUE(ss_6788_1809_dark_mode);
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


void init_options(const fs::path& ini, const std::vector<std::string>& opts);
void dump_options();
void dump_available_options();

fs::path make_temp_file();

}	// namespace

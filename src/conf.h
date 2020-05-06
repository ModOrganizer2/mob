#pragma once

namespace mob
{

#define VALUE(NAME) \
	static decltype(auto) NAME() { return by_name(#NAME); }

#define VALUE_BOOL(NAME) \
	static bool NAME() { return by_name_bool(#NAME); }


struct tools
{
	static const fs::path& by_name(const std::string& s);

	VALUE(perl);
	VALUE(msbuild);
	VALUE(devenv);
	VALUE(cmake);
	VALUE(git);
	VALUE(sevenz);
	VALUE(jom);
	VALUE(patch);
	VALUE(nuget);
	VALUE(vswhere);
	VALUE(vcvars);
};

struct conf
{
	static void set_log_level(int i);

	static bool log_dump();
	static bool log_trace();
	static bool log_debug();
	static bool log_info();
	static bool log_warning();
	static bool log_error();

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

	//VALUE(boost);
};

struct versions
{
	static const std::string& by_name(const std::string& s);

	VALUE(vs);
	VALUE(vs_year);
	VALUE(vs_toolset);
	VALUE(sdk);
	VALUE(sevenzip);
	VALUE(zlib);
	VALUE(boost);
	VALUE(boost_vs);
	VALUE(python);
	VALUE(fmt);
	VALUE(gtest);
	VALUE(libbsarch);
	VALUE(libloot);
	VALUE(libloot_hash);
	VALUE(openssl);
	VALUE(bzip2);
	VALUE(lz4);
	VALUE(nmm);
	VALUE(spdlog);
	VALUE(usvfs);
	VALUE(qt);
	VALUE(qt_vs);
	VALUE(pyqt);
	VALUE(pyqt_builder);
	VALUE(sip);
	VALUE(pyqt_sip);
	VALUE(explorerpp);

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

	VALUE(vs);
	VALUE(qt_install);
	VALUE(qt_bin);
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

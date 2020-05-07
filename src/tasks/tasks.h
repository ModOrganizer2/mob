#pragma once

#include "task.h"
#include "../net.h"
#include "../conf.h"
#include "../utility.h"
#include "../op.h"
#include "../tools/tools.h"

namespace mob
{

class boost : public basic_task<boost>
{
public:
	boost();

	static const std::string& version();
	static const std::string& version_vs();
	static bool prebuilt();

	static fs::path source_path();
	static fs::path lib_path(arch a);
	static fs::path root_lib_path(arch a);

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	void fetch_prebuilt();
	void build_and_install_prebuilt();

	void fetch_from_source();
	void build_and_install_from_source();
	void write_config_jam();

	void bootstrap();

	void do_b2(
		const std::vector<std::string>& components,
		const std::string& link, const std::string& runtime_link, arch a);

	static std::smatch parse_boost_version();
	static std::string source_download_filename();
	static fs::path config_jam_file();
	static url prebuilt_url();
	static url source_url();
	static fs::path b2_exe();
	static std::string python_dll();
	static std::string python_version_for_dll();
	static std::string python_version_for_jam();
	static std::string boost_version_no_patch_underscores();
	static std::string boost_version_no_tags();
	static std::string boost_version_no_tags_underscores();
	static std::string boost_version_all_underscores();
	static std::string address_model_for_arch(arch a);
};


// needed by bsapacker
//
class boost_di : public basic_task<boost_di>
{
public:
	boost_di();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
};


class bzip2 : public basic_task<bzip2>
{
public:
	bzip2();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;

private:
	static url source_url();
};


class explorerpp : public basic_task<explorerpp>
{
public:
	explorerpp();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;

private:
	static url source_url();
};


class fmt : public basic_task<fmt>
{
public:
	fmt();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	static url source_url();
};


class gtest : public basic_task<gtest>
{
public:
	gtest();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;
};


class licenses : public basic_task<licenses>
{
public:
	licenses();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_build_and_install() override;
};


class libbsarch : public basic_task<libbsarch>
{
public:
	libbsarch();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	static std::string dir_name();
	static url source_url();
};


class libffi : public basic_task<libffi>
{
public:
	libffi();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();
	static fs::path include_path();
	static fs::path lib_path();

protected:
	void do_fetch() override;
};


class libloot : public basic_task<libloot>
{
public:
	libloot();

	static const std::string& version();
	static const std::string& hash();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	static std::string dir_name();
	static url source_url();
};


class lz4 : public basic_task<lz4>
{
public:
	lz4();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	static fs::path solution_dir();
	static fs::path solution_file();
	static fs::path out_dir();

	void fetch_prebuilt();
	void build_and_install_prebuilt();

	void fetch_from_source();
	void build_and_install_from_source();

	static url prebuilt_url();
};


class modorganizer : public basic_task<modorganizer>
{
public:
	modorganizer(std::string name);

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

	bool is_super() const override;

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	std::string repo_;

	void initialize_super(const fs::path& super_root);

	fs::path this_source_path() const;

	static fs::path super_path();
};


class ncc : public basic_task<ncc>
{
public:
	ncc();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;
};


class nmm : public basic_task<nmm>
{
public:
	nmm();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;
};


class openssl : public basic_task<openssl>
{
public:
	openssl();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();
	static fs::path include_path();
	static fs::path bin_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	void fetch_prebuilt();
	void fetch_from_source();
	void build_and_install_prebuilt();
	void build_and_install_from_source();

	void configure();
	void install_engines();
	void copy_files();
	void copy_dlls_to(const fs::path& dir);
	void copy_pdbs_to(const fs::path& dir);

	static url source_url();
	static url prebuilt_url();
	static fs::path build_path();
	static std::vector<std::string> output_names();
	static std::smatch parse_version();
	static std::string version_no_tags();
	static std::string version_no_minor_underscores();
};


class pyqt : public basic_task<pyqt>
{
public:
	pyqt();

	static const std::string& version();
	static const std::string& builder_version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	void fetch_prebuilt();
	void fetch_from_source();
	void build_and_install_prebuilt();
	void build_and_install_from_source();

	void sip_build();
	void install_sip_file();
	void copy_files();
	void copy_qt_dlls();

	static url source_url();
	static url prebuilt_url();
	static fs::path sip_install_file();
	static fs::path build_path();
	static std::vector<std::string> modules();
};


class python : public basic_task<python>
{
public:
	struct version_info
	{
		std::string major;
		std::string minor;
		std::string patch;
	};

	python();

	static const std::string& version();
	static bool prebuilt();

	static version_info parsed_version();
	static fs::path source_path();
	static fs::path build_path();
	static fs::path python_exe();
	static fs::path include_path();
	static fs::path scripts_path();
	static fs::path site_packages_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	void fetch_prebuilt();
	void fetch_from_source();
	void build_and_install_prebuilt();
	void build_and_install_from_source();

	void package();
	void install_pip();
	void copy_files();

	static std::string version_without_v();
	static url prebuilt_url();
	static fs::path solution_file();
	static std::string version_for_dll();
};


class sip : public basic_task<sip>
{
public:
	sip();

	static const std::string& version();
	static const std::string& version_for_pyqt();
	static bool prebuilt();

	static fs::path source_path();
	static fs::path sip_module_exe();
	static fs::path sip_install_exe();
	static fs::path module_source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	void download();
	void generate();

	static fs::path download_file();
};


class sevenz : public basic_task<sevenz>
{
public:
	sevenz();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	static url source_url();
	static std::string version_for_url();
	static fs::path module_to_build();

	void build();
};


class spdlog : public basic_task<spdlog>
{
public:
	spdlog();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
};


class stylesheets : public basic_task<stylesheets>
{
public:
	stylesheets();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	struct release
	{
		std::string repo;
		std::string name;
		std::string version;
		std::string file;
	};

	static std::vector<release> releases();
};


class usvfs : public basic_task<usvfs>
{
public:
	usvfs();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	void fetch_prebuilt();
	void fetch_from_source();
	void build_and_install_prebuilt();
	void build_and_install_from_source();

	void download_from_appveyor(arch a);
	void copy_prebuilt(arch a);

	std::string prebuilt_directory_name(arch a);
};


class zlib : public basic_task<zlib>
{
public:
	zlib();

	static const std::string& version();
	static bool prebuilt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	static url source_url();
};

}	// namespace

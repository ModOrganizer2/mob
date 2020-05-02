#pragma once

#include "task.h"
#include "../net.h"
#include "../conf.h"
#include "../tools.h"
#include "../utility.h"

namespace builder
{

class boost : public basic_task<boost>
{
public:
	boost();
	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	void fetch_prebuilt();
	void build_and_install_prebuilt();

	void fetch_from_source();
	void build_and_install_from_source();
	void write_config_jam();

	void do_b2(
		const std::vector<std::string>& components,
		const std::string& link, const std::string& runtime_link, arch a);

	static std::smatch parse_boost_version();
	static std::string source_download_filename();
	static fs::path config_jam_file();
	static url prebuilt_url();
	static url source_url();
	static fs::path lib_path(arch a);
	static std::string python_dll();
	static std::string python_version_for_dll();
	static std::string python_version_for_jam();
	static std::string boost_version_no_patch_underscores();
	static std::string boost_version_no_tags();
	static std::string boost_version_no_tags_underscores();
	static std::string boost_version_all_underscores();
	static std::string address_model_for_arch(arch a);
};


class bzip2 : public basic_task<bzip2>
{
public:
	bzip2();
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
	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	static url source_url();
};


class gtest : public basic_task<gtest>
{
public:
	gtest();
	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
};


class libbsarch : public basic_task<libbsarch>
{
public:
	libbsarch();
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
	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	static fs::path solution_dir();
	static fs::path solution_file();
	static fs::path bin_dir();
};


class ncc : public basic_task<ncc>
{
public:
	ncc();
	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
};


class nmm : public basic_task<nmm>
{
public:
	nmm();
	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
};


class openssl : public basic_task<openssl>
{
public:
	openssl();
	static fs::path source_path();
	static fs::path include_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	void configure();
	void install_engines();
	void copy_files();
	void copy_dlls_to(const fs::path& dir);
	void copy_pdbs_to(const fs::path& dir);

	static fs::path build_path();
	static url source_url();
	static std::vector<std::string> output_names();
	static std::smatch parse_version();
	static std::string version_no_tags();
	static std::string version_no_minor_underscores();
};


class pyqt : public basic_task<pyqt>
{
public:
	pyqt();

	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

	static url source_url();
	static fs::path build_dir();
	static fs::path sip_install_file();
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

	static version_info version();
	static fs::path source_path();
	static fs::path build_path();
	static fs::path python_exe();
	static fs::path include_path();
	static fs::path scripts_path();
	static fs::path site_packages_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	void install_pip();

	static fs::path solution_file();
	static std::string version_for_dll();
};


class sip : public basic_task<sip>
{
public:
	sip();

	static fs::path source_path();
	static fs::path sip_module_exe();
	static fs::path sip_install_exe();
	static fs::path module_source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
};


class sevenz : public basic_task<sevenz>
{
public:
	sevenz();
	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	static url source_url();
	static std::string version_for_url();
	static fs::path module_to_build();
};


class spdlog : public basic_task<spdlog>
{
public:
	spdlog();
	static fs::path source_path();

protected:
	void do_fetch() override;
};


class usvfs : public basic_task<usvfs>
{
public:
	usvfs();
	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;
};


class zlib : public basic_task<zlib>
{
public:
	zlib();
	static fs::path source_path();

protected:
	void do_fetch() override;
	void do_build_and_install() override;

private:
	static url source_url();
};

}	// namespace

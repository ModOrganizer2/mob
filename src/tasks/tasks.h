#pragma once

#include "task.h"
#include "../tools/tools.h"
#include "../net.h"
#include "../utility.h"
#include "../core/conf.h"
#include "../core/op.h"

namespace mob::tasks
{

// single header for all the tasks, not worth having a header per task


class boost : public basic_task<boost>
{
public:
	struct version_info
	{
		std::string major, minor, patch, rest;
	};

	boost();

	static version_info parsed_version();
	static std::string version();
	static std::string version_vs();
	static bool prebuilt();

	static fs::path source_path();
	static fs::path lib_path(arch a);
	static fs::path root_lib_path(arch a);

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

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

	void copy_boost_python_dll();
};


// needed by bsapacker
//
class boost_di : public basic_task<boost_di>
{
public:
	boost_di();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
};


// needed by python
//
class bzip2 : public basic_task<bzip2>
{
public:
	bzip2();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
};


class explorerpp : public basic_task<explorerpp>
{
public:
	explorerpp();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
};


class fmt : public basic_task<fmt>
{
public:
	fmt();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;
};


class gtest : public basic_task<gtest>
{
public:
	gtest();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;
};


class installer : public basic_task<installer>
{
public:
	installer();

	static bool prebuilt();
	static std::string version();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;
};


class libbsarch : public basic_task<libbsarch>
{
public:
	libbsarch();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;
};


// needed by python
//
class libffi : public basic_task<libffi>
{
public:
	libffi();

	static std::string version();
	static bool prebuilt();

	static fs::path source_path();
	static fs::path include_path();
	static fs::path lib_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
};


class libloot : public basic_task<libloot>
{
public:
	libloot();

	static std::string version();
	static std::string hash();
	static std::string branch();

	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;
};


class licenses : public basic_task<licenses>
{
public:
	licenses();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_build_and_install() override;
};


class lz4 : public basic_task<lz4>
{
public:
	lz4();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

private:
	void fetch_prebuilt();
	void build_and_install_prebuilt();

	void fetch_from_source();
	void build_and_install_from_source();

	msbuild create_msbuild_tool(msbuild::ops o=msbuild::build);
};


// a task for all modorganizer projects except for the installer, which is
// technically an MO project but is built differently
//
// note that this doesn't inherit from basic_task because it doesn't have the
// usual static functions for source_path() and prebuilt() since there's a
// variety of modorganizer objects, one per project
//
class modorganizer : public task
{
public:
	// path of the root modorganizer_super directory
	//
	static fs::path super_path();

	// creates the same cmake tool as the one used to build the various MO
	// projects, used internally, but also by the cmake command
	//
	static cmake create_cmake_tool(
		const fs::path& root, cmake::ops o=cmake::generate);


	// flags for some MO projects
	enum flags
	{
		noflags  = 0x00,

		// gamebryo project, used by the translations task because these
		// projects have multiple .ts files that have to be merged
		gamebryo = 0x01,

		// project that uses nuget, cmake doesn't support those right now, so
		// `msbuild -t:restore` has to be run manually
		nuget   = 0x02,
	};

	// some mo tasks have more than one name, mostly because the transifex slugs
	// are different than the names on github; the std::string and const char*
	// overloads are because they're constructed from initializer lists and it's
	// more convenient that way
	modorganizer(std::string name, flags f=noflags);
	modorganizer(std::vector<std::string> names, flags f=noflags);
	modorganizer(std::vector<const char*> names, flags f=noflags);


	// whether this project has the gamebryo flag on
	//
	bool is_gamebryo_plugin() const;

	// whether this project has the nuget flag on
	//
	bool is_nuget_plugin() const;

	// url to the git repo
	//
	url git_url() const;

	// `mo_org` setting from the ini (typically ModOrganizer2)
	//
	std::string org() const;

	// name of the repo on github (first name given in the constructor,
	// something like "cmake_common" or "modorganizer-uibase")
	//
	std::string repo() const;

	// directory for the project's source, something like
	// "build/modorganizer_super/modorganizer"
	//
	fs::path source_path() const;

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

private:
	std::string repo_;
	flags flags_;

	// creates the cmake tool for this MO project
	//
	cmake create_cmake_tool(cmake::ops o=cmake::generate);

	// creates the msbuild tool for this MO project
	//
	msbuild create_msbuild_tool(msbuild::ops o=msbuild::build);

	// this is the file targeted by the msbuild tool
	//
	// it's not actually the .sln file because the cmake files have historically
	// been inconsistent in what the main project in the solution is and whether
	// the INSTALL project was enabled or not, so just building the .sln itself
	// might not actually build everything
	//
	// by targeting the INSTALL project directly, everything will always be
	// built correctly, regardless of how the solution file is generated
	//
	fs::path project_file_path() const;
};


class ncc : public basic_task<ncc>
{
public:
	ncc();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

private:
	msbuild create_msbuild_tool(msbuild::ops o);
};


class nmm : public basic_task<nmm>
{
public:
	nmm();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

private:
	msbuild create_msbuild_tool(
		msbuild::ops o=msbuild::build, msbuild::flags_t f=msbuild::noflags);
};


class openssl : public basic_task<openssl>
{
public:
	struct version_info
	{
		std::string major, minor, patch;
	};

	openssl();

	static version_info parsed_version();
	static std::string version();
	static bool prebuilt();

	static fs::path source_path();
	static fs::path include_path();
	static fs::path build_path();
	static fs::path bin_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

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
};


// when building from source, builds both pyqt5 and pyqt5-sip; when using the
// prebuilt, the downloaded zip contains both
//
class pyqt : public basic_task<pyqt>
{
public:
	pyqt();

	static std::string version();
	static std::string builder_version();
	static bool prebuilt();

	static fs::path source_path();
	static fs::path build_path();

	// "PyQt5.sip", used both in pyqt and sip
	//
	static std::string pyqt_sip_module_name();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

private:
	void fetch_prebuilt();
	void fetch_from_source();
	void build_and_install_prebuilt();
	void build_and_install_from_source();

	void sip_build();
	void install_sip_file();
	void copy_files();
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

	static std::string version();
	static bool prebuilt();
	static version_info parsed_version();

	// build/python-XX
	//
	static fs::path source_path();

	// build/python-XX/PCBuild/amd64
	//
	static fs::path build_path();

	// build/python-XX/PCBuild/amd64/python.exe
	//
	static fs::path python_exe();

	// build/python-XX/Include
	//
	static fs::path include_path();

	// build/python-XX/Scripts
	//
	static fs::path scripts_path();

	// build/python-XX/Lib/site-packages
	//
	static fs::path site_packages_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

private:
	void fetch_prebuilt();
	void fetch_from_source();
	void build_and_install_prebuilt();
	void build_and_install_from_source();

	void package();
	void install_pip();
	void copy_files();

	msbuild create_msbuild_tool(msbuild::ops o=msbuild::build);
};


class sip : public basic_task<sip>
{
public:
	sip();

	static std::string version();
	static std::string version_for_pyqt();
	static bool prebuilt();

	static fs::path source_path();
	static fs::path sip_module_exe();
	static fs::path sip_install_exe();
	static fs::path module_source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

private:
	void download();
	void generate();

	static fs::path download_file();
};


class sevenz : public basic_task<sevenz>
{
public:
	sevenz();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

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

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
};


class stylesheets : public basic_task<stylesheets>
{
public:
	stylesheets();

	static bool prebuilt();
	static fs::path source_path();

	static std::string paper_lad_6788_version();
	static std::string paper_automata_6788_version();
	static std::string paper_mono_6788_version();
	static std::string dark_mode_1809_6788_version();

	// dummy, doesn't apply
	static std::string version();

protected:
	void do_clean(clean c) override;
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

	downloader make_downloader_tool(
		const release& r, downloader::ops=downloader::download) const;

	fs::path release_build_path(const release& r) const;

	static std::vector<release> releases();
};


class translations : public basic_task<translations>
{
public:
	class projects
	{
	public:
		struct lang
		{
			std::string name;
			std::vector<fs::path> ts_files;

			lang(std::string n);
		};

		struct project
		{
			std::string name;
			std::vector<lang> langs;

			project(std::string n);
		};


		projects(fs::path root);

		const std::vector<project>& get() const;
		const std::vector<std::string>& warnings() const;

	private:
		const fs::path root_;
		std::vector<project> projects_;
		std::vector<std::string> warnings_;
		std::set<fs::path> warned_;

		void create();
		bool is_gamebryo_plugin(
			const std::string& dir, const std::string& project);
		void handle_project_dir(const fs::path& dir);

		lang handle_ts_file(
			bool gamebryo, const std::string& project_name, const fs::path& f);
	};


	translations();

	static bool prebuilt();
	static std::string version();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;
};


class usvfs : public basic_task<usvfs>
{
public:
	usvfs();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

private:
	void fetch_prebuilt();
	void fetch_from_source();
	void build_and_install_prebuilt();
	void build_and_install_from_source();

	void download_from_appveyor(arch a);
	void copy_prebuilt(arch a);

	msbuild create_msbuild_tool(arch a, msbuild::ops o=msbuild::build) const;

	std::vector<std::shared_ptr<downloader>> create_appveyor_downloaders(
		arch a, downloader::ops o=downloader::download) const;

	std::string prebuilt_directory_name(arch a) const;
};


class zlib : public basic_task<zlib>
{
public:
	zlib();

	static std::string version();
	static bool prebuilt();
	static fs::path source_path();

protected:
	void do_clean(clean c) override;
	void do_fetch() override;
	void do_build_and_install() override;

private:
	cmake create_cmake_tool(cmake::ops o=cmake::generate);
	msbuild create_msbuild_tool(msbuild::ops o=msbuild::build);

	static url source_url();
};

}	// namespace

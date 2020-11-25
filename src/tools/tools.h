#pragma once

#include "../net.h"
#include "../core/conf.h"
#include "../core/op.h"
#include "../core/context.h"

namespace mob
{

class process;

// all the various tools used by mob itself or the tasks, most of them inherit
// from basic_process_runner, which is a small wrapper around a `process`,
// although `downloader` doesn't because it uses a curl_downloader instead
//
// a tool is meant to be used from within a task with task::run_tool(), but can
// also be run standalone with run(); some tools provide a result() function for
// the tool's output
//
// some tools are also used extensively from the command line, like git, which
// has a wrapper `git_wrap` around the all git operations needed and is also
// used by the `git` tool itself
//
// the `context` class knows about tools and will log the tool name when given
// one


// base class for all tools
//
class tool
{
public:
	// can't copy tools, they have atomics, processes, etc.
	//
	tool(tool&& t);
	tool& operator=(tool&& t);

	virtual ~tool() = default;

	// tool name, set in constructor
	//
	const std::string& name() const;

	// tells the given context that this tool is running and calls do_run()
	//
	void run(context& cx);

	// sets the interrupt flag and calls do_interrupt(); derived classes
	// can call interrupted() regularly to see if they should stop or do
	// something specific in do_interrupt()
	//
	void interrupt();

	// context given in run(), gcx() if none
	//
	const context& cx() const;

	// some tools have a result() member function that returns something
	// specific, like an int of a path; task::run_tool() is a template and
	// always returns result(), so a base implementation is required here
	//
	void result() {}

protected:
	tool(std::string name);

	// whether this tool should stop executing as soon as possible
	//
	bool interrupted() const;

	// called from run(), does the actual work
	//
	virtual void do_run() = 0;

	// called from interrupt(), only called once
	//
	virtual void do_interrupt();

	// changes the name given in the constructor, some tool can have more
	// specific or useful names later once they get more parameters
	//
	void set_name(const std::string& s);

private:
	// set in run(), can be null
	context* cx_;

	// tool name from constructor or set_name()
	//
	std::string name_;

	// true when interrupt() is called
	//
	std::atomic<bool> interrupted_;
};


// these are not strictly tools, but they're a centralized place for perl,
// nasm, qt and vswhere stuff

// only used when building openssl from source
//
struct perl
{
	// path to perl binary
	//
	static fs::path binary();
};

// only used when building openssl from source, bundled with mob in
// third-party/bin
//
struct nasm
{
	// path to nasm binary
	//
	static fs::path binary();
};

struct qt
{
	// path to qt's root directory, the one that contains bin, include, etc.
	//
	static fs::path installation_path();

	// path to qt's bin directory, basically installation_path()/bin
	//
	static fs::path bin_path();

	// qt version from the ini
	//
	static std::string version();

	// vs version for qt as a year, used to build the msvcYYYY_xx directory name
	//
	static std::string vs_version();
};

// bundled with mob in third-party/bin
//
struct vswhere
{
	// runs the vswhere binary and returns the output, empty on error
	//
	// note that vswhere may return more than one line if there are multiple
	// installations of vs found; this is handled in find_vs() in paths.cpp
	//
	static std::string find_vs();
};


// a tool that downloads a file, can be given multiple urls in case one fails;
// if none of the given urls can be downloaded, bails out
//
// if file() is not called, the downloader will use the filename from the url
// and put the file in the cache directory (the downloads/ directory by default)
//
// in any case, if the output file already exists, the file is not downloaded
// and run() returns immediately; result() can be used to figure out the path
// of the file
//
class downloader : public tool
{
public:
	// what run() should do
	//
	enum ops
	{
		// deletes the file at the path where it would be downloaded
		clean = 1,

		// downloads the file to the given path
		download
	};

	// an empty downloader, url() must be called
	//
	downloader(ops o=download);

	// a downloader for the given url
	//
	downloader(mob::url u, ops o=download);

	// adds a url to download
	//
	downloader& url(const mob::url& u);

	// output file
	//
	downloader& file(const fs::path& p);

	// path to the output file; this is file() if it was called, or the
	// generated name if it wasn't, which can vary if multiple urls were given
	//
	fs::path result() const;

protected:
	// cleans or downloads
	//
	void do_run() override;

	// tells the curl_downloader to stop
	//
	void do_interrupt() override;

private:
	// given in the constructor
	ops op_;

	// the curl downloader
	std::unique_ptr<curl_downloader> dl_;

	// output path, may be empty when run() is called, will contain the
	// generated filename later
	fs::path file_;

	// every url added with url()
	std::vector<mob::url> urls_;


	// deletes an already downloaded file, no-op if not found
	//
	void do_clean();

	// downloads a file to the output path
	//
	void do_download();

	// generates an output path for the given url
	//
	fs::path path_for_url(const mob::url& u) const;

	// checks if the file exists
	//
	bool try_picking(const fs::path& file);
};


// base class for tools that run processes
//
class basic_process_runner : public tool
{
public:
	// anchors
	basic_process_runner(basic_process_runner&&);
	~basic_process_runner();

	// changes the name of this tool to the process' name, gives the tool's
	// context to the process, and runs and joins it, returning the exit code
	//
	// does not copy the process
	//
	// tools normally use the protected set_process() and execute_and_join(),
	// but this is used by git_wrap, which can be run standalone from the
	// command line, or reused by the `git` class in tasks
	//
	// when git_wrap is used for tasks, it's given the `git` tool used by the
	// task and calls this function to run processes in the context of the tool;
	// when standalone, it just executes process objects directly
	//
	int execute_and_join(process& p);

	// exit code of the process
	//
	int exit_code() const;

protected:
	basic_process_runner(std::string name);

	// makes a copy of the given process and stores it
	//
	void set_process(const process& p);

	// returns the internal process copied from set_process(), used by derived
	// classes to get information about the process, like the output
	//
	process& get_process();

	// interrupts the internal process
	//
	void do_interrupt() override;

	// executes the internal process and waits until it terminates
	//
	int execute_and_join();

private:
	std::unique_ptr<process> process_;
};


// runs an arbitrary process, used by some tasks for one-time programs that
// don't have a dedicated tool class, like b2 for boost,
//
// does not copy the given process, which must live at least until run() returns
//
class process_runner : public tool
{
public:
	// creates a process_runner for the given process, keeps a reference to it
	//
	process_runner(process& p);

	// anchor
	~process_runner();

	// returns the exit code of process, returned by task::run_tool()
	//
	int result() const;

protected:
	// changes the name of this tool to the process' name, gives the tool's
	// context to the process, and runs and joins it
	//
	void do_run() override;

	// interrupts the process
	//
	void do_interrupt() override;

private:
	// process given in constructor
	process& p_;
};


// tool to handle extracting archives
//
// if extraction fails, an interruption file is left in the directory so
// extraction is restarted next time mob runs
//
// 7z is bundled with mob in third-party/bin
//
class extractor : public basic_process_runner
{
public:
	// path to the program used for extraction, typically 7z
	//
	static fs::path binary();


	extractor();

	// file to extract
	//
	extractor& file(const fs::path& file);

	// output directory
	//
	extractor& output(const fs::path& dir);

protected:
	// extracts the file
	//
	void do_run() override;

private:
	fs::path file_;
	fs::path where_;

	// some archives have a top level directory, this moves all the files up one
	// directory and deletes the now empty top level directory
	//
	void check_duplicate_directory(const fs::path& ifile);
};


// tool to handle creating archives, 7z is bundled with mob in third-party/bin
//
// this isn't used by any task, but it's used in a few places in op.cpp, mostly
// for creating archives with the `release` command
//
class archiver : public basic_process_runner
{
public:
	// archives all the files matching `glob` into a file `out`, ignoring
	// anything that matches a string in `ignore`
	//
	static void create_from_glob(
		const context& cx, const fs::path& out,
		const fs::path& glob, const std::vector<std::string>& ignore);

	// archives all the given files rooted in `files_root`, into a file `out`
	//
	static void create_from_files(
		const context& cx, const fs::path& out,
		const std::vector<fs::path>& files, const fs::path& files_root);
};


// tool to apply patches available for a third-party task; patches live in
// mob/patches, in one directory per task
//
// patch.exe is bundled with mob in third-party/bin
//
// there are two ways of patching a task:
//
//  1) auto patching: each task runs the patcher tool as a step in
//     task::fetch(), just after calling do_fetch(), which checks for a
//     directory `patches/task-name/sources` or `patches/task-name/prebuilt`
//     depending on whether this is a prebuilt or not
//
//     any file ending with `.patch` will be applied, rooted in the path
//     given to root() (defaults to the task's source directory)
//
//  2) manual patching: a task can run the patcher with a specific patch file
//     given in file() at any time, which is useful for tasks that need to
//     patch files generated later in the building process, such as pyqt
//
//     manual patch files can be anywhere, but they're in patches/task-name with
//     a `.manual_patch` by convention
//
class patcher : public basic_process_runner
{
public:
	// path to the patch binary
	//
	static fs::path binary();


	patcher();

	// sets the task name and whether it's a prebuilt, used in task::fetch() for
	// auto patching
	//
	patcher& task(const std::string& name, bool prebuilt=false);

	// sets the patch's filename; if this is not set, uses the task name and
	// prebuilt flag from task()
	//
	patcher& file(const fs::path& p);

	// directory in which the patches are applied; the filenames in the patch
	// files must be relative to this directory
	//
	patcher& root(const fs::path& dir);

protected:
	// applies the patch file from the task directory or the patch file given
	// in file()
	//
	void do_run() override;

private:
	// task name and prebuilt flag, used to generate the auto patching directory
	std::string task_;
	bool prebuilt_;

	// root directory, used to resolve the relative file names in the patch
	// files
	fs::path root_;

	// patch file, empty for auto patching
	fs::path file_;


	// applies one patch file
	//
	void do_patch(const fs::path& patch_file);
};


// tool that runs `jom`, an alternative to `nmake` that supports parallel
// builds, bundled with mob in third-party/bin
//
// note that although jom reduces build times considerably, some third parties
// don't handle it very well, giving a bunch of errors about files in use, etc.
//
// so most tools that use jom use a loop that runs jom several times, allowing
// failure, and then run jom one last time in single_job mode to finish the
// build, which should be guaranteed to work
//
class jom : public basic_process_runner
{
public:
	// path to the job binary
	//
	static fs::path binary();


	enum flags_t
	{
		noflags        = 0x00,

		// disables multi-process build
		single_job     = 0x01,

		// don't bail out on failure
		allow_failure  = 0x02
	};

	jom();

	// path in which to invoke jom
	//
	jom& path(const fs::path& p);

	// makefile target
	//
	jom& target(const std::string& s);

	// adds a macro definition as "NAME=value"
	//
	jom& def(const std::string& s);

	// sets flags
	//
	jom& flag(flags_t f);

	// sets the architecture used to run jom, used to get the appropriate
	// VS environment variables, defaults to arch::def
	//
	jom& architecture(arch a);

	// jom's exit code
	//
	int result() const;

protected:
	// runs jom
	//
	void do_run() override;

private:
	// set in path()
	fs::path cwd_;

	// set in def(), just plain arguments
	std::vector<std::string> def_;

	// set in target(), another plain argument
	std::string target_;

	// set in flag()
	flags_t flags_;

	// set in architecture()
	arch arch_;
};


// tool that runs devenv.exe, only invoked to upgrade projects for now
//
class vs : public basic_process_runner
{
public:
	// path to devenv.exe
	//
	static fs::path devenv_binary();

	// path to visual studio's root directory, the one that contains Common7,
	// VC, etc.
	//
	static fs::path installation_path();

	// path to vswhere.exe, typically from mob's third-party/bin
	//
	static fs::path vswhere();

	// path to vcvars batch file
	//
	static fs::path vcvars();

	// vs version from ini
	//
	static std::string version();

	// vs year from ini
	//
	static std::string year();

	// vs toolset from ini
	//
	static std::string toolset();

	// vs sdk version from ini
	//
	static std::string sdk();


	enum ops
	{
		upgrade = 1
	};

	vs(ops o);

	// path to the solution file to be upgraded
	//
	vs& solution(const fs::path& sln);

protected:
	// calls do_upgrade()
	//
	void do_run() override;

private:
	ops op_;
	fs::path sln_;

	// upgrades the solution file
	//
	void do_upgrade();
};


// tool that runs `nuget restore` for the given solution, bundled with mob in
// third-party/bin
//
class nuget : public basic_process_runner
{
public:
	// nuget tool for the given solution
	//
	nuget(fs::path sln);

	// path to the nuget binary
	//
	static fs::path binary();

protected:
	// runs nuget
	void do_run() override;

private:
	// solution file set in constructor
	fs::path sln_;
};


// tool that runs python
//
class python : public basic_process_runner
{
public:
	python();

	// working directory
	//
	python& root(const fs::path& p);

	// arbitrary arguments given to python
	//
	python& arg(const std::string& s);

protected:
	// runs python
	//
	void do_run() override;

private:
	// working directory
	fs::path root_;

	// arguments
	std::vector<std::string> args_;
};


// tool that runs pip for one of the operations in the enum
//
class pip : public basic_process_runner
{
public:
	enum ops
	{
		// installs pip if needed and updates it
		ensure = 1,

		// runs `-m pip install` for the given package/version or file
		//
		install,

		// runs `-m pip download` for the given package and version, does not
		//  use file()
		download
	};


	// a pip tool for the given operation
	//
	pip(ops o);

	// `pip install` can work with either a "package==version" string, which
	// downloads the file if needed, or a local file
	//
	// so for the `install` operation, either package/version or file can be
	// given, but the `download` operation requires the package/version only
	// and puts the downloaded file in the "cache" path (defaults to the
	// downloads/ directory)
	//
	pip& package(const std::string& s);
	pip& version(const std::string& s);
	pip& file(const fs::path& p);

protected:
	// runs pip
	//
	void do_run() override;

private:
	// what pip command to run
	ops op_;

	// set in package(), version() and file()
	std::string package_;
	std::string version_;
	fs::path file_;

	// runs `-m ensurepip`, then upgrades pip
	//
	void do_ensure();

	// runs `-m pip install` with the given package/version or file
	//
	void do_install();

	// runs `-m pip download` with the given package/version, saves the file
	// in the cache directory
	//
	void do_download();
};


// tool that runs transifex, used for pulling translations before release,
// bundled with mob in third-party/bin
//
class transifex : public basic_process_runner
{
public:
	// path to the tx binary
	//
	static fs::path binary();


	enum ops
	{
		// runs `ini`, initializes an empty directory to use transifex
		init = 1,

		// runs `config`, sets the url
		config,

		// pulls translations
		pull
	};

	transifex(ops op);

	// directory that contains the .tx directory
	//
	transifex& root(const fs::path& p);

	// api key used for requests
	//
	transifex& api_key(const std::string& key);

	// url of the translations on transifex
	//
	transifex& url(const mob::url& u);

	// minimum completion percentage of translations to pull, must be an int
	// between 0 and 100
	//
	transifex& minimum(int percent);

	// sets the log level for tx's stdout; it's trace by default while building,
	// but this tool is also used on the command line, in which case stdout is
	// set to info to see the logs on the console
	//
	transifex& stdout_level(context::level lv);

	// passes --force to `pull`, bypasses timestamp checks and forces redownload
	// of all translation files
	//
	transifex& force(bool b);

protected:
	// runs the selected operation
	//
	void do_run() override;

private:
	// operation
	ops op_;

	// log level of stdout, set in stdout_level()
	context::level stdout_;

	// root directory
	fs::path root_;

	// api key
	std::string key_;

	// url of translations on transifex
	mob::url url_;

	// minimum completion percentage, [0,100]
	int min_;

	// whether to give `--force` to `pull`
	bool force_;


	// runs `init`
	//
	void do_init();

	// runs `config`
	//
	void do_config();

	// runs `pull`
	//
	void do_pull();
};


// tool that runs `lrelease`, which compiles translation files into a .qm file
// and drops them in the directory given in out(), typically
// install/bin/translations
//
// lrelease.exe from the qt installation
//
class lrelease : public basic_process_runner
{
public:
	// path to the lrelease binary
	//
	static fs::path binary();

	lrelease();

	// name of the project, used to generate the output filename, which is
	// project_lang.qm
	//
	lrelease& project(const std::string& name);

	// adds or sets the source .ts files that are compiled to create the .qm
	// file
	//
	// a .qm file is normally compiled from just one .ts, but some projects use
	// an additional .ts file, like gamebryo, where each game plugin has
	// specific strings in their own .ts file, but also use a common .ts file
	// from gamebryo
	//
	// this prevents copying common strings to all the plugin .ts files, giving
	// more work to translators, but it also means that lrelease must know about
	// both the plugin's .ts and gamebryo's ts
	//
	// it's also assumed that all the given .ts files are for the same language,
	// so the filename of the first source that's added is used as the language
	// string when generating the filename, along with the project name
	//
	// since .ts files from transifex are simply named `lang.ts`, such as
	// `fr.ts` or `de.ts`, the generated filename would be something like
	// `uibase_fr.qm`
	//
	lrelease& add_source(const fs::path& ts_file);
	lrelease& sources(const std::vector<fs::path>& v);

	// output directory where the .qm file is generated
	//
	lrelease& out(const fs::path& dir);

	// path to the .qm file that was generated
	//
	fs::path qm_file() const;

protected:
	// runs lrelease on the given sources
	//
	void do_run() override;

private:
	// project name
	std::string project_;

	// .ts files
	std::vector<fs::path> sources_;

	// output directory
	fs::path out_;
};


// tool that runs Inno Setup's iscc.exe to create the installer
//
class iscc : public basic_process_runner
{
public:
	// path to the iscc.exe binary
	//
	static fs::path binary();

	// iscc tool with an optional path to the .iss file
	//
	iscc(fs::path iss={});

	// .iss file
	//
	iscc& iss(const fs::path& p);

protected:
	// runs iscc
	//
	void do_run() override;

private:
	// path to the .iss file
	fs::path iss_;
};

}	// namespace


// more tools
#include "git.h"
#include "cmake.h"
#include "msbuild.h"

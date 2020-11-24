#pragma once

#include "../net.h"
#include "../core/conf.h"
#include "../core/op.h"
#include "../core/context.h"

namespace mob
{

class process;
class git;

class tool
{
public:
	tool(tool&& t);
	tool& operator=(tool&& t);

	virtual ~tool() = default;

	const std::string& name() const;

	void run(context& cx);
	void interrupt();

	void result() {}

protected:
	tool(std::string name);

	const context& cx() const;

	bool interrupted() const;

	virtual void do_run() = 0;
	virtual void do_interrupt() = 0;

	void set_name(const std::string& s);

private:
	context* cx_;
	std::string name_;
	std::atomic<bool> interrupted_;
};


struct perl
{
	static fs::path binary();
};

struct nasm
{
	static fs::path binary();
};

struct qt
{
	static fs::path installation_path();
	static fs::path bin_path();
	static std::string version();
	static std::string vs_version();
};


class downloader : public tool
{
public:
	enum ops
	{
		clean = 1,
		download
	};

	downloader(ops o=download);
	downloader(mob::url u, ops o=download);

	downloader& url(const mob::url& u);
	downloader& file(const fs::path& p);

	fs::path result() const;

protected:
	void do_run() override;
	void do_interrupt() override;

private:
	ops op_;
	std::unique_ptr<curl_downloader> dl_;
	fs::path file_;
	std::vector<mob::url> urls_;

	void do_clean();
	void do_download();

	fs::path path_for_url(const mob::url& u) const;
	bool try_picking(const fs::path& file);
};


class basic_process_runner : public tool
{
public:
	// anchors
	basic_process_runner(basic_process_runner&&);
	~basic_process_runner();

	void join();
	int exit_code() const;

protected:
	basic_process_runner(std::string name);

	void set_process(const process& p);
	process& get_process();

	void do_interrupt() override;
	int execute_and_join();

private:
	std::unique_ptr<process> process_;
};


class process_runner : public tool
{
public:
	process_runner(process&& p);
	process_runner(process& p);

	// anchor
	~process_runner();

	void join();
	int exit_code() const;

	int result() const;

protected:
	void do_run() override;

private:
	std::unique_ptr<process> own_;
	process* p_;

	void do_interrupt() override;
	int execute_and_join();

	process& real_process();
	const process& real_process() const;
};




class git_submodule_adder : public instrumentable<2>
{
public:
	enum class times
	{
		add_submodule_wait,
		add_submodule
	};

	~git_submodule_adder();

	static git_submodule_adder& instance();

	void queue(git g);
	void stop();

private:
	struct sleeper
	{
		std::mutex m;
		std::condition_variable cv;
		bool ready = false;
	};

	context cx_;
	std::thread thread_;
	std::vector<git> queue_;
	mutable std::mutex queue_mutex_;
	std::atomic<bool> quit_;
	sleeper sleeper_;

	git_submodule_adder();
	void run();

	void thread_fun();
	void wakeup();
	void process();
};


class extractor : public basic_process_runner
{
public:
	extractor();

	static fs::path binary();

	extractor& file(const fs::path& file);
	extractor& output(const fs::path& dir);

protected:
	void do_run() override;

private:
	fs::path file_;
	fs::path where_;

	void check_duplicate_directory(const fs::path& ifile);
};


class archiver : public basic_process_runner
{
public:
	static void create_from_glob(
		const context& cx, const fs::path& out,
		const fs::path& glob, const std::vector<std::string>& ignore);

	static void create_from_files(
		const context& cx, const fs::path& out,
		const std::vector<fs::path>& files, const fs::path& files_root);
};


class patcher : public basic_process_runner
{
public:
	patcher();

	static fs::path binary();

	patcher& task(const std::string& name, bool prebuilt=false);
	patcher& file(const fs::path& p);
	patcher& root(const fs::path& dir);

protected:
	void do_run() override;

private:
	std::string task_;
	bool prebuilt_;
	fs::path output_;
	fs::path file_;

	void do_patch(const fs::path& patch_file);
};


class cmake : public basic_process_runner
{
public:
	enum generators
	{
		vs    = 0x01,
		jom   = 0x02
	};

	enum ops
	{
		generate =1 ,
		clean
	};

	cmake(ops o=generate);

	static fs::path binary();

	cmake& generator(generators g);
	cmake& generator(const std::string& g);
	cmake& root(const fs::path& p);
	cmake& output(const fs::path& p);
	cmake& prefix(const fs::path& s);
	cmake& def(const std::string& name, const std::string& value);
	cmake& def(const std::string& name, const fs::path& p);
	cmake& def(const std::string& name, const char* s);
	cmake& architecture(arch a);
	cmake& cmd(const std::string& s);

	// returns the path given in output(), if it was set
	//
	// if not, returns the build path based on the parameters (for example,
	// `vsbuild_32/` for a 32-bit arch with the VS generator
	//
	fs::path build_path() const;

	// returns build_path(), used by task::run_tool()
	//
	fs::path result() const;

protected:
	void do_run() override;

private:
	struct gen_info
	{
		std::string dir;
		std::string name;
		std::string x86;
		std::string x64;

		std::string get_arch(arch a) const;
		std::string output_dir(arch a) const;
	};

	ops op_;
	fs::path root_;
	generators gen_;
	std::string genstring_;
	fs::path prefix_;
	std::vector<std::pair<std::string, std::string>> def_;
	fs::path output_;
	arch arch_;
	std::string cmd_;

	void do_clean();
	void do_generate();

	static const std::map<generators, gen_info>& all_generators();
	static const gen_info& get_generator(generators g);
};


class jom : public basic_process_runner
{
public:
	enum flags_t
	{
		noflags        = 0x00,
		single_job     = 0x01,
		allow_failure  = 0x02
	};

	jom();

	static fs::path binary();

	jom& path(const fs::path& p);
	jom& target(const std::string& s);
	jom& def(const std::string& s);
	jom& flag(flags_t f);
	jom& architecture(arch a);

	int result() const;

protected:
	void do_run() override;

private:
	fs::path cwd_;
	std::vector<std::string> def_;
	std::string target_;
	flags_t flags_;
	arch arch_;
};


class msbuild : public basic_process_runner
{
public:
	enum flags_t
	{
		noflags        = 0x00,
		single_job     = 0x01,
		allow_failure  = 0x02
	};

	enum ops
	{
		build = 1,
		clean
	};

	msbuild(ops o=build);

	static fs::path binary();

	msbuild& solution(const fs::path& sln);
	msbuild& targets(const std::vector<std::string>& names);
	msbuild& parameters(const std::vector<std::string>& params);
	msbuild& config(const std::string& s);
	msbuild& platform(const std::string& s);
	msbuild& architecture(arch a);
	msbuild& flags(flags_t f);
	msbuild& prepend_path(const fs::path& p);

	int result() const;

protected:
	void do_run() override;

private:
	ops op_;
	fs::path sln_;
	std::vector<std::string> targets_;
	std::vector<std::string> params_;
	std::string config_;
	std::string platform_;
	arch arch_;
	flags_t flags_;
	std::vector<fs::path> prepend_path_;

	void do_clean();
	void do_build();
	void do_run(const std::vector<std::string>& targets);
};

MOB_ENUM_OPERATORS(msbuild::flags_t);


class vs : public basic_process_runner
{
public:
	enum ops
	{
		upgrade = 1
	};

	vs(ops o);

	static fs::path devenv_binary();
	static fs::path installation_path();
	static fs::path vswhere();
	static fs::path vcvars();
	static std::string version();
	static std::string year();
	static std::string toolset();
	static std::string sdk();

	vs& solution(const fs::path& sln);

protected:
	void do_run() override;

private:
	ops op_;
	fs::path sln_;

	void do_upgrade();
};


class vswhere : public basic_process_runner
{
public:
	static fs::path find_vs();
};


class nuget : public basic_process_runner
{
public:
	nuget(fs::path sln);

	static fs::path binary();

protected:
	void do_run() override;

private:
	fs::path sln_;
};


class python : public basic_process_runner
{
public:
	python();

	python& root(const fs::path& p);
	python& arg(const std::string& s);

protected:
	void do_run() override;

private:
	fs::path root_;
	std::vector<std::string> args_;
};


class pip : public basic_process_runner
{
public:
	enum ops
	{
		ensure = 1,
		install,
		download
	};

	pip(ops o);

	pip& package(const std::string& s);
	pip& version(const std::string& s);
	pip& file(const fs::path& p);

protected:
	void do_run() override;

private:
	ops op_;
	std::string package_;
	std::string version_;
	fs::path file_;

	void do_ensure();
	void do_install();
	void do_download();
};


class transifex : public basic_process_runner
{
public:
	enum ops
	{
		init = 1,
		config,
		pull
	};

	static fs::path binary();


	transifex(ops op);

	transifex& root(const fs::path& p);
	transifex& api_key(const std::string& key);
	transifex& url(const mob::url& u);
	transifex& minimum(int percent);
	transifex& stdout_level(context::level lv);
	transifex& force(bool b);

protected:
	void do_run() override;

private:
	ops op_;
	context::level stdout_;
	fs::path root_;
	std::string key_;
	mob::url url_;
	int min_;
	bool force_;

	void do_init();
	void do_config();
	void do_pull();
};


class lrelease : public basic_process_runner
{
public:
	static fs::path binary();

	lrelease();

	lrelease& project(const std::string& name);
	lrelease& add_source(const fs::path& ts_file);
	lrelease& sources(const std::vector<fs::path>& v);
	lrelease& out(const fs::path& dir);

	fs::path qm_file() const;

protected:
	void do_run() override;

private:
	std::string project_;
	std::vector<fs::path> sources_;
	fs::path out_;
};


class iscc : public basic_process_runner
{
public:
	static fs::path binary();

	iscc(fs::path iss = {});

	iscc& iss(const fs::path& p);

protected:
	void do_run() override;

private:
	fs::path iss_;
};

}	// namespace


#include "git.h"

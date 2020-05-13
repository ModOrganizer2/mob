#pragma once

#include "../net.h"
#include "../process.h"
#include "../conf.h"
#include "../op.h"
#include "../context.h"

namespace mob
{

class tool
{
public:
	tool(tool&& t);
	tool& operator=(tool&& t);

	virtual ~tool() = default;

	std::string name() const;

	void run(context& cx);
	void interrupt();

	void result() {}

protected:
	context* cx_;

	tool(std::string name);

	bool interrupted() const;

	virtual void do_run() = 0;
	virtual void do_interrupt() = 0;
	virtual std::string do_name() const { return {}; }

private:
	std::string name_;
	std::atomic<bool> interrupted_;
};



class downloader : public tool
{
public:
	downloader();
	downloader(mob::url u);

	downloader& url(const mob::url& u);
	downloader& file(const fs::path& p);

	fs::path result() const;

protected:
	void do_run() override;
	void do_interrupt() override;

private:
	std::unique_ptr<curl_downloader> dl_;
	fs::path file_;
	std::vector<mob::url> urls_;

	fs::path path_for_url(const mob::url& u) const;
	bool try_picking(const fs::path& file);
};


class basic_process_runner : public tool
{
public:
	std::string do_name() const override;
	void join();
	int exit_code() const;

protected:
	process process_;

	basic_process_runner(std::string name={});

	void do_interrupt() override;
	int execute_and_join();
};


class process_runner : public tool
{
public:
	process_runner(process&& p);
	process_runner(process& p);

	std::string do_name() const override;
	void join();
	int exit_code() const;

	int result() const;

protected:
	void do_run() override;

private:
	process own_;
	process* p_;

	void do_interrupt() override;
	int execute_and_join();

	process& real_process();
	const process& real_process() const;
};


class git_clone : public basic_process_runner
{
public:
	git_clone();

	git_clone& url(const mob::url& u);
	git_clone& branch(const std::string& name);
	git_clone& output(const fs::path& dir);

protected:
	void do_run() override;

private:
	mob::url url_;
	std::string branch_;
	fs::path where_;

	void clone();
	void pull();
};


class extractor : public basic_process_runner
{
public:
	extractor();
	extractor& file(const fs::path& file);
	extractor& output(const fs::path& dir);

protected:
	void do_run() override;

private:
	fs::path file_;
	fs::path where_;

	void check_duplicate_directory(const fs::path& ifile);
};


class patcher : public basic_process_runner
{
public:
	patcher();

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

	cmake();

	static void clean(const context& cx, const fs::path& root);

	cmake& generator(generators g);
	cmake& root(const fs::path& p);
	cmake& prefix(const fs::path& s);
	cmake& def(const std::string& name, const std::string& value);
	cmake& def(const std::string& name, const fs::path& p);
	cmake& def(const std::string& name, const char* s);
	cmake& architecture(arch a);

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

	fs::path root_;
	generators gen_;
	fs::path prefix_;
	fs::path output_;
	arch arch_;

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

	jom& path(const fs::path& p);
	jom& target(const std::string& s);
	jom& def(const std::string& s);
	jom& flag(flags_t f);
	jom& architecture(arch a);

	int result() const;

protected:
	void do_run() override;

private:
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

	msbuild();

	msbuild& solution(const fs::path& sln);
	msbuild& projects(const std::vector<std::string>& names);
	msbuild& parameters(const std::vector<std::string>& params);
	msbuild& config(const std::string& s);
	msbuild& platform(const std::string& s);
	msbuild& architecture(arch a);
	msbuild& flags(flags_t f);

	int result() const;

protected:
	void do_run() override;

private:
	fs::path sln_;
	std::vector<std::string> projects_;
	std::vector<std::string> params_;
	std::string config_;
	std::string platform_;
	arch arch_;
	flags_t flags_;

	void write_custom_props_file();
};


class devenv_upgrade : public basic_process_runner
{
public:
	devenv_upgrade(fs::path sln);

protected:
	void do_run() override;

private:
	fs::path sln_;
};


class nuget : public basic_process_runner
{
public:
	nuget(fs::path sln);

protected:
	void do_run() override;

private:
	fs::path sln_;
};


class pip_install : public basic_process_runner
{
public:
	pip_install();

	pip_install& package(const std::string& s);
	pip_install& version(const std::string& s);
	pip_install& file(const fs::path& p);

protected:
	void do_run() override;

private:
	std::string package_;
	std::string version_;
	fs::path file_;
};

}	// namespace

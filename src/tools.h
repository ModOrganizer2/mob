#pragma once

#include "net.h"
#include "op.h"

namespace builder
{

void vcvars();


class tool
{
public:
	virtual ~tool() = default;

	void run();
	void interrupt();

protected:
	tool(std::string name);

	bool interrupted() const;
	virtual void do_run() = 0;
	virtual void do_interrupt() = 0;

private:
	std::string name_;
	std::atomic<bool> interrupted_;
};



class downloader : public tool
{
public:
	downloader(url u);
	downloader(std::vector<url> urls);

	fs::path file() const;

protected:
	void do_run() override;
	void do_interrupt() override;

private:
	curl_downloader dl_;
	fs::path file_;
	std::vector<url> urls_;
};


class process_runner : public tool
{
public:
	process_runner(std::string name, std::string cmd, fs::path cwd={});

	void join(bool check_exit_code=true);
	int exit_code() const;

protected:
	process_runner(std::string name);

	void do_run() override;
	void do_interrupt() override;
	int execute_and_join(process p, bool check_exit_code=true);

private:
	std::string cmd_;
	fs::path cwd_;
	process process_;
};


class git_clone : public process_runner
{
public:
	git_clone(
		std::string author, std::string repo, std::string b, fs::path where);

protected:
	void do_run() override;

private:
	std::string author_;
	std::string repo_;
	std::string branch_;
	fs::path where_;

	void clone();
	void pull();
	url repo_url() const;
};


class decompresser : public process_runner
{
public:
	decompresser(fs::path file, fs::path where);

protected:
	void do_run() override;

private:
	fs::path file_;
	fs::path where_;

	fs::path interrupt_file() const;
	void check_duplicate_directory();
};


class patcher : public process_runner
{
public:
	patcher(fs::path patch_dir, fs::path output_dir);

protected:
	void do_run() override;

private:
	fs::path patches_;
	fs::path output_;
};


class cmake_for_nmake : public process_runner
{
public:
	cmake_for_nmake(fs::path root, std::string args={}, fs::path prefix={});
	static fs::path build_path();

protected:
	void do_run() override;

private:
	fs::path root_;
	std::string args_;
	fs::path prefix_;
};


class cmake_for_vs : public process_runner
{
public:
	cmake_for_vs(fs::path root, std::string args={}, fs::path prefix={});
	static fs::path build_path();

protected:
	void do_run() override;

private:
	fs::path root_;
	std::string args_;
	fs::path prefix_;
};


class jom : public process_runner
{
public:
	enum flags
	{
		default_flags  = 0x00,
		single_job     = 0x01,
		accept_failure = 0x02
	};

	jom(
		fs::path dir, std::string target, std::string args,
		flags f=default_flags);

protected:
	void do_run() override;

private:
	fs::path dir_;
	std::string target_;
	std::string args_;
	flags flags_;
};

}	// namespace

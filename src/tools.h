#pragma once

#include "net.h"
#include "op.h"

namespace builder
{

void vcvars();
fs::path find_sevenz();

class tool
{
public:
	virtual ~tool() = default;

	void run();
	void interrupt();
	void result() {}

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
	fs::path result() const;

protected:
	void do_run() override;
	void do_interrupt() override;

private:
	curl_downloader dl_;
};


class process_runner : public tool
{
public:
	process_runner(std::string name, std::string cmd, fs::path cwd={});

protected:
	process_runner(std::string name);

	void do_run() override;
	void do_interrupt() override;
	void set(process p);

private:
	std::string cmd_;
	fs::path cwd_;
	process process_;
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
};


class cmake_for_nmake : public process_runner
{
public:
	cmake_for_nmake(fs::path root, std::string args, fs::path prefix={});
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
	cmake_for_vs(fs::path root, std::string args, fs::path prefix={});
	static fs::path build_path();

protected:
	void do_run() override;

private:
	fs::path root_;
	std::string args_;
	fs::path prefix_;
};


class nmake : public process_runner
{
public:
	nmake(fs::path dir, std::string args={});

protected:
	void do_run() override;

private:
	fs::path dir_;
	std::string args_;
};


class nmake_install : public process_runner
{
public:
	nmake_install(fs::path dir, std::string args={});

protected:
	void do_run() override;

private:
	fs::path dir_;
	std::string args_;
};

}	// namespace

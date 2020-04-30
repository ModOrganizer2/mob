#pragma once

#include "net.h"
#include "op.h"

namespace builder
{

void vcvars();


class tool
{
public:
	tool(tool&& t);
	tool& operator=(tool&& t);

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
	downloader();
	downloader(builder::url u);

	downloader& url(const builder::url& u);

	fs::path result() const;

protected:
	void do_run() override;
	void do_interrupt() override;

private:
	std::unique_ptr<curl_downloader> dl_;
	fs::path file_;
	std::vector<builder::url> urls_;

	fs::path path_for_url(const builder::url& u) const;
};


class basic_process_runner : public tool
{
public:
	void join(bool check_exit_code=true);
	int exit_code() const;

protected:
	basic_process_runner(std::string name);

	void do_interrupt() override;

	int execute_and_join(process p, bool check_exit_code=true);
	int execute_and_join(const cmd& c, bool check_exit_code=true);

private:
	std::string cmd_;
	fs::path cwd_;
	process process_;
};


class process_runner : public basic_process_runner
{
public:
	process_runner(fs::path exe, cmd::flags flags)
		: basic_process_runner(exe.filename().string()), cmd_(exe, flags)
	{
	}

	process_runner& name(const std::string& s)
	{
		cmd_.name(s);
		return *this;
	}

	const std::string& name() const
	{
		return cmd_.name();
	}

	process_runner& cwd(const fs::path& p)
	{
		cmd_.cwd(p);
		return *this;
	}

	const fs::path& cwd() const
	{
		return cmd_.cwd();
	}

	template <class... Args>
	process_runner& arg(Args&&... args)
	{
		cmd_.arg(std::forward<Args>(args)...);
		return *this;
	}

	int result() const
	{
		return exit_code();
	}

protected:
	void do_run() override
	{
		execute_and_join(cmd_);
	}

private:
	cmd cmd_;
};


class git_clone : public basic_process_runner
{
public:
	git_clone();

	git_clone& org(const std::string& name);
	git_clone& repo(const std::string& name);
	git_clone& branch(const std::string& name);
	git_clone& output(const fs::path& dir);

protected:
	void do_run() override;

private:
	std::string org_;
	std::string repo_;
	std::string branch_;
	fs::path where_;

	void clone();
	void pull();
	url repo_url() const;
};


class decompresser : public basic_process_runner
{
public:
	decompresser();
	decompresser& file(const fs::path& file);
	decompresser& output(const fs::path& dir);

protected:
	void do_run() override;

private:
	fs::path file_;
	fs::path where_;

	fs::path interrupt_file() const;
	void check_duplicate_directory();
};


class patcher : public basic_process_runner
{
public:
	patcher();

	patcher& task(const std::string& name);
	patcher& root(const fs::path& dir);

protected:
	void do_run() override;

private:
	fs::path patches_;
	fs::path output_;
};


class cmake : public basic_process_runner
{
public:
	enum generators
	{
		vs    = 0x01,
		nmake = 0x02
	};

	cmake();

	cmake& generator(generators g);
	cmake& root(const fs::path& p);
	cmake& prefix(const fs::path& s);
	cmake& def(const std::string& s);

	fs::path result() const;

protected:
	void do_run() override;

private:
	fs::path root_;
	generators gen_;
	fs::path prefix_;
	fs::path output_;
	cmd cmd_;
};


class jom : public basic_process_runner
{
public:
	enum flags
	{
		noflags        = 0x00,
		single_job     = 0x01,
		accept_failure = 0x02
	};

	jom();

	jom& path(const fs::path& p);
	jom& target(const std::string& s);
	jom& def(const std::string& s);
	jom& flag(flags f);

	int result() const;

protected:
	void do_run() override;

private:
	cmd cmd_;
	std::string target_;
	flags flags_;
};


class msbuild : public basic_process_runner
{
public:
	msbuild();

	msbuild& solution(const fs::path& sln);
	msbuild& projects(const std::vector<std::string>& names);
	msbuild& parameters(const std::vector<std::string>& params);

protected:
	void do_run() override;

private:
	fs::path sln_;
	std::vector<std::string> projects_;
	std::vector<std::string> params_;
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

}	// namespace

#pragma once

#include "utility.h"

namespace builder
{

class process
{
public:
	process();
	process(std::string cmd, handle_ptr h);

	process(process&&);
	process& operator=(process&&);
	~process();

	process(const process&) = delete;
	process& operator=(const process&) = delete;

	void interrupt();
	void join();

	const std::string& cmd() const;
	int exit_code() const;

private:
	std::string cmd_;
	handle_ptr handle_;
	std::atomic<bool> interrupt_;
	DWORD code_;
};


class op
{
public:
	static void touch(const fs::path& p);
	static void create_directories(const fs::path& p);
	static void delete_directory(const fs::path& p);
	static void delete_file(const fs::path& p);
	static void remove_readonly(const fs::path& first);
	static void copy_file_to_dir_if_better(const fs::path& file, const fs::path& dir);
	static process run(const std::string& cmd, const fs::path& cwd={});

private:
	static void do_touch(const fs::path& p);
	static void do_create_directories(const fs::path& p);
	static void do_delete_directory(const fs::path& p);
	static void do_delete_file(const fs::path& p);
	static void do_copy_file_to_dir(const fs::path& f, const fs::path& d);
	static void do_remove_readonly(const fs::path& p);
	static process do_run(const std::string& cmd, const fs::path& cwd);
	static void check(const fs::path& p);
};

}	// namespace

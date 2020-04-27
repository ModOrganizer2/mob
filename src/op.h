#pragma once

namespace builder
{

class op
{
public:
	static void create_directories(const fs::path& p);
	static void delete_directory(const fs::path& p);
	static void delete_file(const fs::path& p);
	static void remove_readonly(const fs::path& first);
	static void copy_file_to_dir(const fs::path& file, const fs::path& dir);
	static void run(const std::string& cmd, const fs::path& cwd={});

private:
	static void do_create_directories(const fs::path& p);
	static void do_delete_directory(const fs::path& p);
	static void do_delete_file(const fs::path& p);
	static void do_copy_file_to_dir(const fs::path& f, const fs::path& d);
	static void do_remove_readonly(const fs::path& p);
	static int do_run(const std::string& cmd, const fs::path& cwd);
	static void check(const fs::path& p);
};

}	// namespace

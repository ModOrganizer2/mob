#pragma once

namespace builder
{

struct versions
{
	static std::string vs_year();
	static std::string vs_version();
	static std::string sevenzip();
	static std::string zlib();
};

struct paths
{
	static fs::path prefix();
	static fs::path cache();
	static fs::path build();
	static fs::path install();
	static fs::path install_dlls();

	static fs::path program_files_x86();
	static fs::path temp_dir();
	static fs::path temp_file();
};

struct conf
{
	static bool verbose();
	static bool dry();
};

}	// namespace

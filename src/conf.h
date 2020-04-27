#pragma once

namespace builder
{

struct versions
{
	static const std::string& vs();
	static const std::string& vs_year();
	static const std::string& sevenzip();
	static const std::string& zlib();
	static const std::string& boost();
	static const std::string& boost_vs();
	static const std::string& python();
};

struct paths
{
	static fs::path prefix();
	static fs::path cache();
	static fs::path build();
	static fs::path install();
	static fs::path install_bin();
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

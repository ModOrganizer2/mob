#pragma once

namespace builder
{

void vcvars();
fs::path find_sevenz();

void decompress(const fs::path& file, const fs::path& where);


class cmake_for_nmake
{
public:
	static fs::path build_path();
	fs::path run(const fs::path& root, const std::string& args, const fs::path& prefix={});
};

class cmake_for_vs
{
public:
	static fs::path build_path();
	fs::path run(const fs::path& root, const std::string& args, const fs::path& prefix={});
};


void nmake(const fs::path& dir, const std::string& args={});
void nmake_install(const fs::path& dir, const std::string& args={});

}	// namespace

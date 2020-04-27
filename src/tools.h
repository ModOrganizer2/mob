#pragma once

namespace builder
{

void vcvars();
fs::path find_sevenz();

void expand(const fs::path& file, const fs::path& where);

void cmake(const fs::path& build, const fs::path& prefix, const std::string& generator);
fs::path cmake_for_nmake(const fs::path& root, const fs::path& prefix={});
fs::path cmake_for_vs(const fs::path& root, const fs::path& prefix={});

void nmake(const fs::path& dir);

}	// namespace

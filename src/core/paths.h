#pragma once

namespace mob
{

fs::path find_root(bool verbose=false);
fs::path find_in_root(const fs::path& file);

fs::path find_third_party_directory();
fs::path find_program_files_x86();
fs::path find_program_files_x64();
fs::path find_vs();
fs::path find_qt();
fs::path find_iscc();
fs::path find_temp_dir();

void find_vcvars();
void validate_qt();

fs::path mob_exe_path();

}	// namespace

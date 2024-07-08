#pragma once

namespace mob {

    // returns the path to mob.exe that's currently running, including filename;
    // bails on failure
    //
    fs::path mob_exe_path();

    // returns mob's root directory, contains third-party/, etc.; bails on failure
    //
    // this is not necessarily the parent of mob_exe_path(), it mob could be running
    // from the build directory
    //
    fs::path find_root(bool verbose = false);

    // resolves the given relative path against find_root(); bails when not found
    //
    fs::path find_in_root(const fs::path& file);

    // returns the absolute path of mob's third-party directory; bails when not
    // found
    //
    fs::path find_third_party_directory();

    // returns the absolute path to x86/x64 program files directory
    //
    fs::path find_program_files_x86();
    fs::path find_program_files_x64();

    // returns the absolute path to visual studio's root directory, the one that
    // contains Common7, VC, etc.; bails if not found
    //
    fs::path find_vs();

    // returns the absolute path to VCPKG root directory to be used as VCPKG_ROOT when
    // building
    //
    fs::path find_vcpkg();

    // returns the absolute path to Qt's root directory, the one that contains
    // bin, include, etc.; bails if not found
    //
    fs::path find_qt();

    // returns the absolute path to iscc.exe; bails if not found
    //
    fs::path find_iscc();

    // returns the absolute path to the system's temp directory; bails on error
    //
    fs::path find_temp_dir();

    // returns the absolute path to the vcvars batch file, bails if not found
    //
    fs::path find_vcvars();

}  // namespace mob

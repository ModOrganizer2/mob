#include "pch.h"
#include "paths.h"
#include "../tasks/task_manager.h"
#include "../tasks/tasks.h"
#include "../utility/string.h"

namespace mob {

    // returns a path to the given known folder, empty on error
    //
    fs::path get_known_folder(const GUID& id)
    {
        wchar_t* buffer = nullptr;
        const auto r    = ::SHGetKnownFolderPath(id, 0, 0, &buffer);

        if (r != S_OK)
            return {};

        fs::path p = buffer;
        ::CoTaskMemFree(buffer);

        return p;
    }

    // searches PATH for the given executable, returns empty if not found
    //
    fs::path find_in_path(std::string_view exe)
    {
        const std::wstring wexe = utf8_to_utf16(exe);

        const std::size_t size   = MAX_PATH;
        wchar_t buffer[size + 1] = {};

        if (SearchPathW(nullptr, wexe.c_str(), nullptr, size, buffer, nullptr) == 0)
            return {};

        return buffer;
    }

    // checks if a path exists that starts with `check` and ends with as many parts
    // as possible
    //
    // for example:
    //
    //   try_parts("c:/", {"1", "2", "3"})
    //
    // will try in order:
    //
    //   c:/1/2/3
    //   c:/2/3
    //   c:/3
    //
    // if none of the paths exist, returns false; if one of the paths exists,
    // `check` is set to it and returns true
    //
    bool try_parts(fs::path& check, const std::vector<std::string>& parts)
    {
        for (std::size_t i = 0; i < parts.size(); ++i) {
            fs::path p = check;

            for (std::size_t j = i; j < parts.size(); ++j)
                p /= parts[j];

            gcx().trace(context::conf, "trying parts {}", p);

            if (fs::exists(p)) {
                check = p;
                return true;
            }
        }

        return false;
    }

    // looks for `qmake.exe` in the given path, tries a variety of subdirectories
    //
    bool find_qmake(fs::path& check)
    {
        // try Qt/Qt5.14.2/msvc*/bin/qmake.exe
        if (try_parts(check, {"Qt", "Qt" + qt::version(),
                              "msvc" + qt::vs_version() + "_64", "bin", "qmake.exe"})) {
            return true;
        }

        // try Qt/5.14.2/msvc*/bin/qmake.exe
        if (try_parts(check, {"Qt", qt::version(), "msvc" + qt::vs_version() + "_64",
                              "bin", "qmake.exe"})) {
            return true;
        }

        return false;
    }

    fs::path mob_exe_path()
    {
        const int max_tries = 3;

        DWORD buffer_size = MAX_PATH;

        for (int tries = 0; tries < max_tries; ++tries) {
            auto buffer = std::make_unique<wchar_t[]>(buffer_size + 1);
            DWORD n     = GetModuleFileNameW(0, buffer.get(), buffer_size);

            if (n == 0) {
                const auto e = GetLastError();

                gcx().bail_out(context::conf, "can't get module filename, {}",
                               error_message(e));
            }
            else if (n >= buffer_size) {
                // buffer is too small, try again
                buffer_size *= 2;
            }
            else {
                // if GetModuleFileName() works, `n` does not include the null
                // terminator
                const std::wstring s(buffer.get(), n);
                return fs::canonical(s);
            }
        }

        gcx().bail_out(context::conf, "can't get module filename");
    }

    fs::path find_root(bool verbose)
    {
        gcx().trace(context::conf, "looking for root directory");

        fs::path mob_exe_dir = mob_exe_path().parent_path();

        auto third_party = mob_exe_dir / "third-party";

        if (!fs::exists(third_party)) {
            // doesn't exist, maybe this is the build directory

            auto p = mob_exe_dir;

            if (p.filename().u8string() == u8"x64") {
                p = p.parent_path();

                if (p.filename().u8string() == u8"Debug" ||
                    p.filename().u8string() == u8"Release") {
                    if (verbose)
                        u8cout << "mob.exe is in its build directory, looking up\n";

                    // mob_exe_dir is in the build directory
                    third_party = mob_exe_dir / ".." / ".." / ".." / "third-party";
                }
            }
        }

        if (!fs::exists(third_party))
            gcx().bail_out(context::conf, "root directory not found");

        const auto p = fs::canonical(third_party.parent_path());
        gcx().trace(context::conf, "found root directory at {}", p);

        return p;
    }

    fs::path find_in_root(const fs::path& file)
    {
        static fs::path root = find_root();

        fs::path p = root / file;
        if (!fs::exists(p))
            gcx().bail_out(context::conf, "{} not found", p);

        gcx().trace(context::conf, "found {}", p);
        return p;
    }

    fs::path find_third_party_directory()
    {
        static fs::path path = find_in_root("third-party");
        return path;
    }

    fs::path find_program_files_x86()
    {
        fs::path p = get_known_folder(FOLDERID_ProgramFilesX86);

        if (p.empty()) {
            const auto e = GetLastError();

            p = fs::path(R"(C:\Program Files (x86))");

            gcx().warning(
                context::conf,
                "failed to get x86 program files folder, defaulting to {}, {}", p,
                error_message(e));
        }
        else {
            gcx().trace(context::conf, "x86 program files is {}", p);
        }

        return p;
    }

    fs::path find_program_files_x64()
    {
        fs::path p = get_known_folder(FOLDERID_ProgramFilesX64);

        if (p.empty()) {
            const auto e = GetLastError();

            p = fs::path(R"(C:\Program Files)");

            gcx().warning(
                context::conf,
                "failed to get x64 program files folder, defaulting to {}, {}", p,
                error_message(e));
        }
        else {
            gcx().trace(context::conf, "x64 program files is {}", p);
        }

        return p;
    }

    fs::path find_vs()
    {
        // asking vswhere
        const auto output = vswhere::find_vs();
        if (output.empty())
            gcx().bail_out(context::conf, "vswhere failed");

        const auto lines = split(output, "\r\n");

        if (lines.empty()) {
            gcx().bail_out(context::conf, "vswhere didn't output anything");
        }
        else if (lines.size() > 1) {
            gcx().error(context::conf, "vswhere returned multiple installations:");

            for (auto&& line : lines)
                gcx().error(context::conf, " - {}", line);

            gcx().bail_out(
                context::conf,
                "specify the `vs` path in the `[paths]` section of the INI, or "
                "pass -s paths/vs=PATH` to pick an installation");
        }
        else {
            // only one line
            const fs::path path(output);

            if (!fs::exists(path)) {
                gcx().bail_out(context::conf,
                               "the path given by vswhere doesn't exist: {}", path);
            }

            return path;
        }
    }

    fs::path find_vcpkg()
    {
        const auto env_path = this_env::get().get("VCPKG_ROOT");

        if (!env_path.empty()) {
            return fs::absolute(env_path);
        }

        const auto vs_path       = conf().path().vs();
        const auto vcpkg_vs_path = vs_path / "VC" / "vcpkg";
        if (!exists(vcpkg_vs_path)) {
            gcx().bail_out(context::conf, "vcpkg is not part of VS installation at {}",
                           vs_path);
        }

        return vcpkg_vs_path;
    }

    fs::path find_qt()
    {
        // check from the ini first
        fs::path p = conf().path().qt_install();

        if (!p.empty()) {
            p = fs::absolute(p);

            // check if qmake exists in there
            if (find_qmake(p)) {
                p = fs::absolute(p.parent_path() / "..");
                return p;
            }

            // fail early, don't try to guess if the user had something in the ini
            gcx().bail_out(context::conf, "no qt install in {}", p);
        }

        // a list of possible location
        std::deque<fs::path> locations = {conf().path().pf_x64(), "C:\\", "D:\\"};

        // look for qmake in PATH, which is in %qt%/version/msvc.../bin
        fs::path qmake = find_in_path("qmake.exe");
        if (!qmake.empty())
            locations.push_front(qmake.parent_path() / "../../");

        // look for qtcreator.exe in PATH, which is in %qt%/Tools/QtCreator/bin
        fs::path qtcreator = find_in_path("qtcreator.exe");
        if (!qtcreator.empty())
            locations.push_front(qtcreator.parent_path() / "../../../");

        // check each location
        for (fs::path loc : locations) {
            loc = fs::absolute(loc);

            // check for qmake in there
            if (find_qmake(loc)) {
                loc = fs::absolute(loc.parent_path() / "..");
                return loc;
            }
        }

        gcx().bail_out(context::conf, "can't find qt install");
    }

    fs::path find_iscc()
    {
        // don't bother if the installer isn't enabled, it might fail anyway
        if (!task_manager::instance().find_one("installer")->enabled())
            return {};

        // check from the ini first, supports both relative and absolute
        const auto iscc = conf().tool().get("iscc");

        if (iscc.is_absolute()) {
            if (!fs::exists(iscc)) {
                gcx().bail_out(context::conf,
                               "{} doesn't exist (from ini, absolute path)", iscc);
            }

            return iscc;
        }

        // path is relative

        // check in PATH
        fs::path p = find_in_path(path_to_utf8(iscc));
        if (fs::exists(p))
            return fs::canonical(fs::absolute(p));

        // check known installation paths for a bunch of versions
        for (int v : {5, 6, 7, 8}) {
            const fs::path inno_dir = std::format("Inno Setup {}", v);

            // check for both architectures
            for (fs::path pf : {conf().path().pf_x86(), conf().path().pf_x64()}) {
                p = pf / inno_dir / iscc;
                if (fs::exists(p))
                    return fs::canonical(fs::absolute(p));
            }
        }

        gcx().bail_out(context::conf, "can't find {} anywhere", iscc);
    }

    fs::path find_temp_dir()
    {
        const std::size_t buffer_size = MAX_PATH + 2;
        wchar_t buffer[buffer_size]   = {};

        if (GetTempPathW(static_cast<DWORD>(buffer_size), buffer) == 0) {
            const auto e = GetLastError();
            gcx().bail_out(context::conf, "can't get temp path", error_message(e));
        }

        fs::path p(buffer);
        gcx().trace(context::conf, "temp dir is {}", p);

        return p;
    }

    fs::path find_vcvars()
    {
        // check from the ini first
        fs::path bat = conf().tool().get("vcvars");

        if (conf().global().dry()) {
            if (bat.empty())
                bat = "vcvars.bat";

            return bat;
        }

        if (bat.empty()) {
            // derive from vs installation
            bat = vs::installation_path() / "VC" / "Auxiliary" / "Build" /
                  "vcvarsall.bat";
        }

        if (!fs::exists(bat))
            gcx().bail_out(context::conf, "vcvars not found at {}", bat);

        if (bat.is_relative())
            bat = fs::absolute(bat);

        bat = fs::canonical(bat);
        gcx().trace(context::conf, "using vcvars at {}", bat);

        return bat;
    }

}  // namespace mob

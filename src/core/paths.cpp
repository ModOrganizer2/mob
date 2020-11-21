#include "pch.h"
#include "paths.h"
#include "../tasks/tasks.h"
#include "../utility/string.h"

namespace mob
{

fs::path find_in_path(std::string_view exe)
{
	const std::wstring wexe = utf8_to_utf16(exe);

	const std::size_t size = MAX_PATH;
	wchar_t buffer[size + 1] = {};

	if (SearchPathW(nullptr, wexe.c_str(), nullptr, size, buffer, nullptr))
		return buffer;
	else
		return {};
}

fs::path find_iscc()
{
	const auto tasks = find_tasks("installer");
	MOB_ASSERT(tasks.size() == 1);

	if (!tasks[0]->enabled())
		return {};

	auto iscc = conf().tool().get("iscc");
	if (iscc.is_absolute())
	{
		if (!fs::exists(iscc))
		{
			gcx().bail_out(context::conf,
				"{} doesn't exist (from ini, absolute path)", iscc);
		}

		return iscc;
	}

	fs::path p = find_in_path(path_to_utf8(iscc));
	if (fs::exists(p))
		return fs::canonical(fs::absolute(p));

	for (int v : {5, 6, 7, 8})
	{
		const fs::path inno = fmt::format("inno setup {}", v);

		for (fs::path pf : {conf().path().pf_x86(), conf().path().pf_x64()})
		{
			p = pf / inno / iscc;
			if (fs::exists(p))
				return fs::canonical(fs::absolute(p));
		}
	}

	gcx().bail_out(context::conf, "can't find {} anywhere", iscc);
}

bool try_parts(fs::path& check, const std::vector<std::string>& parts)
{
	for (std::size_t i=0; i<parts.size(); ++i)
	{
		fs::path p = check;

		for (std::size_t j=i; j<parts.size(); ++j)
			p /= parts[j];

		gcx().trace(context::conf, "trying parts {}", p);

		if (fs::exists(p))
		{
			check = p;
			return true;
		}
	}

	return false;
}

fs::path mob_exe_path()
{
	// double the buffer size 10 times
	const int max_tries = 10;

	DWORD buffer_size = MAX_PATH;

	for (int tries=0; tries<max_tries; ++tries)
	{
		auto buffer = std::make_unique<wchar_t[]>(buffer_size + 1);
		DWORD n = GetModuleFileNameW(0, buffer.get(), buffer_size);

		if (n == 0) {
			const auto e = GetLastError();
			gcx().bail_out(context::conf,
				"can't get module filename, {}", error_message(e));
		}
		else if (n >= buffer_size) {
			// buffer is too small, try again
			buffer_size *= 2;
		} else {
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

	if (!fs::exists(third_party))
	{
		if (verbose)
			u8cout << "no third-party here\n";

		auto p = mob_exe_dir;

		if (p.filename().u8string() == u8"x64")
		{
			p = p.parent_path();
			if (p.filename().u8string() == u8"Debug" ||
				p.filename().u8string() == u8"Release")
			{
				if (verbose)
					u8cout << "this is mob's build directory, looking up\n";

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

bool find_qmake(fs::path& check)
{
	// try Qt/Qt5.14.2/msvc*/bin/qmake.exe
	if (try_parts(check, {
		"Qt",
		"Qt" + qt::version(),
		"msvc" + qt::vs_version() + "_64",
		"bin",
		"qmake.exe"}))
	{
		return true;
	}

	// try Qt/5.14.2/msvc*/bin/qmake.exe
	if (try_parts(check, {
		"Qt",
		qt::version(),
		"msvc" + qt::vs_version() + "_64",
		"bin",
		"qmake.exe"}))
	{
		return true;
	}

	return false;
}

bool try_qt_location(fs::path& check)
{
	if (!find_qmake(check))
		return false;

	check = fs::absolute(check.parent_path() / "..");
	return true;
}

fs::path find_qt()
{
	fs::path p = conf().path().qt_install();

	if (!p.empty())
	{
		p = fs::absolute(p);

		if (!try_qt_location(p))
			gcx().bail_out(context::conf, "no qt install in {}", p);

		return p;
	}


	std::vector<fs::path> locations =
	{
		conf().path().pf_x64(),
		"C:\\",
		"D:\\"
	};

	// look for qmake, which is in %qt%/version/msvc.../bin
	fs::path qmake = find_in_path("qmake.exe");
	if (!qmake.empty())
		locations.insert(locations.begin(), qmake.parent_path() / "../../");

	// look for qtcreator.exe, which is in %qt%/Tools/QtCreator/bin
	fs::path qtcreator = find_in_path("qtcreator.exe");
	if (!qtcreator.empty())
		locations.insert(locations.begin(), qtcreator.parent_path() / "../../../");

	for (fs::path loc : locations)
	{
		loc = fs::absolute(loc);
		if (try_qt_location(loc))
			return loc;
	}

	gcx().bail_out(context::conf, "can't find qt install");
}

void validate_qt()
{
	fs::path p = qt::installation_path();

	if (!try_qt_location(p))
		gcx().bail_out(context::conf, "qt path {} doesn't exist", p);

	details::set_string("paths", "qt_install", path_to_utf8(p));
}

fs::path get_known_folder(const GUID& id)
{
	wchar_t* buffer = nullptr;
	const auto r = ::SHGetKnownFolderPath(id, 0, 0, &buffer);

	if (r != S_OK)
		return {};

	fs::path p = buffer;
	::CoTaskMemFree(buffer);

	return p;
}

fs::path find_program_files_x86()
{
	fs::path p = get_known_folder(FOLDERID_ProgramFilesX86);

	if (p.empty())
	{
		const auto e = GetLastError();

		p = fs::path(R"(C:\Program Files (x86))");

		gcx().warning(context::conf,
			"failed to get x86 program files folder, defaulting to {}, {}",
			p, error_message(e));
	}
	else
	{
		gcx().trace(context::conf, "x86 program files is {}", p);
	}

	return p;
}

fs::path find_program_files_x64()
{
	fs::path p = get_known_folder(FOLDERID_ProgramFilesX64);

	if (p.empty())
	{
		const auto e = GetLastError();

		p = fs::path(R"(C:\Program Files)");

		gcx().warning(context::conf,
			"failed to get x64 program files folder, defaulting to {}, {}",
			p, error_message(e));
	}
	else
	{
		gcx().trace(context::conf, "x64 program files is {}", p);
	}

	return p;
}

fs::path find_temp_dir()
{
	const std::size_t buffer_size = MAX_PATH + 2;
	wchar_t buffer[buffer_size] = {};

	if (GetTempPathW(static_cast<DWORD>(buffer_size), buffer) == 0)
	{
		const auto e = GetLastError();
		gcx().bail_out(context::conf, "can't get temp path", error_message(e));
	}

	fs::path p(buffer);
	gcx().trace(context::conf, "temp dir is {}", p);

	return p;
}

fs::path find_vs()
{
	if (conf::dry())
		return vs::vswhere();

	const auto path = vswhere::find_vs();
	if (path.empty())
		gcx().bail_out(context::conf, "vswhere failed");

	const auto lines = split(path_to_utf8(path), "\r\n");

	if (lines.empty())
	{
		gcx().bail_out(context::conf, "vswhere didn't output anything");
	}
	else if (lines.size() > 1)
	{
		gcx().error(context::conf, "vswhere returned multiple installations:");

		for (auto&& line : lines)
			gcx().error(context::conf, " - {}", line);

		gcx().bail_out(context::conf,
			"specify the `vs` path in the `[paths]` section of the INI, or "
			"pass -s paths/vs=PATH` to pick an installation");
	}

	if (!fs::exists(path))
	{
		gcx().bail_out(context::conf,
			"the path given by vswhere doesn't exist: {}", path);
	}

	return path;
}

bool try_vcvars(fs::path& bat)
{
	if (!fs::exists(bat))
		return false;

	bat = fs::canonical(fs::absolute(bat));
	return true;
}

void find_vcvars()
{
	fs::path bat = conf().tool().get("vcvars");

	if (conf::dry())
	{
		if (bat.empty())
			bat = "vcvars.bat";

		return;
	}
	else
	{
		if (bat.empty())
		{
			bat = vs::installation_path()
				/ "VC" / "Auxiliary" / "Build" / "vcvarsall.bat";

			if (!try_vcvars(bat))
				gcx().bail_out(context::conf, "vcvars not found at {}", bat);
		}
		else
		{
			if (!try_vcvars(bat))
				gcx().bail_out(context::conf, "vcvars not found at {}", bat);
		}
	}

	details::set_string("tools", "vcvars", path_to_utf8(bat));
	gcx().trace(context::conf, "using vcvars at {}", bat);
}

}	// namespace

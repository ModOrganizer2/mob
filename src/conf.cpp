#include "pch.h"
#include "conf.h"
#include "utility.h"

namespace builder
{

std::string versions::vs_year() { return "2019"; }
std::string versions::vs_version() { return "16"; }
std::string versions::sevenzip() { return "19.00"; }
std::string versions::zlib() { return "1.2.11"; }


fs::path paths::prefix() { return R"(C:\dev\projects\mobuild-out)"; }
fs::path paths::cache() { return prefix() / "downloads"; }
fs::path paths::build() { return prefix() / "build"; }
fs::path paths::install() { return prefix() / "install"; }

fs::path paths::install_dlls() { return install() / "dlls"; }

fs::path paths::program_files_x86()
{
	static fs::path path = []
	{
		wchar_t* buffer = nullptr;

		const auto r = ::SHGetKnownFolderPath(
			FOLDERID_ProgramFilesX86, 0, 0, &buffer);

		fs::path p;

		if (r == S_OK)
		{
			p = buffer;
			::CoTaskMemFree(buffer);
		}
		else
		{
			const auto e = GetLastError();
			error("failed to get program files folder", e);
			p = fs::path(R"(C:\Program Files (x86))");
		}

		debug("program files is '" + p.string() + "'");
		return p;
	}();

	return path;
}

fs::path paths::temp_dir()
{
	static fs::path temp_dir = []
	{
		const std::size_t buffer_size = MAX_PATH + 2;
		wchar_t buffer[buffer_size] = {};

		if (GetTempPathW(static_cast<DWORD>(buffer_size), buffer) == 0)
		{
			const auto e = GetLastError();
			bail_out("can't get temp path", e);
		}

		fs::path p(buffer);
		debug("temp dir is " + p.string());
		return p;
	}();

	return temp_dir;
}

fs::path paths::temp_file()
{
	static fs::path dir = temp_dir();

	wchar_t name[MAX_PATH + 1] = {};
	if (GetTempFileNameW(dir.native().c_str(), L"mo_", 0, name) == 0)
	{
		const auto e = GetLastError();
		bail_out("can't create temp file in " + dir.string(), e);
	}

	return dir / name;
}


bool conf::verbose()
{
	return false;
}

bool conf::dry()
{
	return false;
}


fs::path find_sevenz()
{
	static fs::path path = []
	{
		const fs::path sevenz = "7z.exe";
		const fs::path third_party = "third-party/bin";

		fs::path final;

		if (fs::path p=third_party / sevenz; fs::exists(p))
			final = fs::absolute(p);
		else if(p = ".." / third_party / sevenz; fs::exists(p))
			final = fs::absolute(p);
		else if (p = "../../.." / third_party / sevenz; fs::exists(p))
			final = fs::absolute(p);

		if (final.empty())
			bail_out("7z.exe not found");

		debug("found " + final.string());
		return final;
	}();

	return path;
}

}	// namespace

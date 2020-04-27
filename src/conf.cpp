#include "pch.h"
#include "conf.h"
#include "utility.h"

namespace builder
{

static std::map<std::string, std::string> g_conf =
{
	{"vs",       "16"},
	{"vs_year",  "2019"},
	{"sevenzip", "19.00"},
	{"zlib",     "1.2.11"},
	{"boost",    "1.72.0"},
	{"boost_vs", "14.2"},
	{"python",   "3.8.1"},
	{"fmt",      "6.1.2"},

	{"prefix",   R"(C:\dev\projects\mobuild-out)"},
	{"downloads", "downloads"},
	{"build",     "build"},
	{"install",   "install"},
	{"bin",       "bin"},
	{"dlls",      "dlls"},
};

const std::string& get_conf(const std::string& name)
{
	auto itor = g_conf.find(name);
	if (itor == g_conf.end())
		bail_out("conf '" + name + "' doesn't exist");

	return itor->second;
}


const std::string& versions::vs()       { return get_conf("vs"); }
const std::string& versions::vs_year()  { return get_conf("vs_year"); }
const std::string& versions::sevenzip() { return get_conf("sevenzip"); }
const std::string& versions::zlib()     { return get_conf("zlib"); }
const std::string& versions::boost()    { return get_conf("boost"); }
const std::string& versions::boost_vs() { return get_conf("boost_vs"); }
const std::string& versions::python()   { return get_conf("python"); }
const std::string& versions::fmt()      { return get_conf("fmt"); }

fs::path paths::prefix()         { return get_conf("prefix"); }
fs::path paths::cache()          { return prefix() / get_conf("downloads"); }
fs::path paths::build()          { return prefix() / get_conf("build"); }
fs::path paths::install()        { return prefix() / get_conf("install"); }
fs::path paths::install_bin( )   { return install() / get_conf("bin"); }
fs::path paths::install_dlls()   { return install_bin() / get_conf("dlls"); }


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

		debug("program files is " + p.string());
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
	return true;
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

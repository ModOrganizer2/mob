#include "pch.h"

namespace builder
{

namespace fs = std::filesystem;

class bailed {};
const bool dry = false;
const bool verbose = false;


std::string error_message(DWORD e)
{
	return std::error_code(
		static_cast<int>(e), std::system_category()).message();
}

void out(const std::string& s)
{
	static std::mutex m;
	std::scoped_lock lock(m);
	std::cout << s << "\n";
}

void out(const std::string& s, DWORD e)
{
	out(s + ", " + error_message(e));
}

void out(const std::string& s, const std::error_code& e)
{
	out(s + ", " + e.message());
}

template <class... Args>
[[noreturn]] void bail_out(Args&&... args)
{
	out(std::forward<Args>(args)...);
	throw bailed();
}

template <class... Args>
void error(Args&&... args)
{
	out(std::forward<Args>(args)...);
}

template <class... Args>
void info(Args&&... args)
{
	out(std::forward<Args>(args)...);
}

template <class... Args>
void debug(Args&&... args)
{
	if (verbose)
		out(std::forward<Args>(args)...);
}


std::string redir_nul()
{
	if (verbose)
		return {};
	else
		return "> NUL";
}


std::string read_text_file(const fs::path& p)
{
	debug("reading " + p.string());

	std::string s;

	std::ifstream in(p);
	if (!in)
		bail_out("can't read from " + p.string() + "'");

	in.seekg(0, std::ios::end);
	s.resize(static_cast<std::size_t>(in.tellg()));
	in.seekg(0, std::ios::beg);
	in.read(&s[0], static_cast<std::streamsize>(s.size()));

	return s;
}

std::string replace_all(
	std::string s, const std::string& from, const std::string& to)
{
	for (;;)
	{
		const auto pos = s.find(from);
		if (pos == std::string::npos)
			break;

		s.replace(pos, from.size(), to);
	}

	return s;
}


struct versions
{
	static const std::string vs_year() { return "2019"; }
	static const std::string vs_version() { return "16"; }
	static const std::string sevenzip() { return "19.00"; }
	static const std::string zlib() { return "1.2.11"; }
};

struct paths
{
	static fs::path prefix() { return R"(C:\dev\projects\mobuild-out)"; }
	static fs::path cache() { return prefix() / "downloads"; }
	static fs::path build() { return prefix() / "build"; }
	static fs::path install() { return prefix() / "install"; }

	static fs::path install_dlls() { return install() / "dlls"; }

	static fs::path program_files_x86()
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

	static fs::path temp_dir()
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

	static fs::path temp_file()
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
};


class op
{
public:
	static void create_directories(const fs::path& p)
	{
		debug("creating directory " + p.string());
		check(p);

		if (!dry)
			do_create_directories(p);
	}

	static void delete_directory(const fs::path& p)
	{
		debug("deleting directory " + p.string());
		check(p);

		if (fs::exists(p) && !fs::is_directory(p))
			bail_out(p.string() + " is not a directory");

		if (!dry)
			do_delete_directory(p);
	}

	static void delete_file(const fs::path& p)
	{
		debug("deleting file " + p.string());
		check(p);

		if (fs::exists(p) && !fs::is_regular_file(p))
			bail_out("can't delete " + p.string() + ", not a file");

		if (!dry)
			do_delete_file(p);
	}

	static void remove_readonly(const fs::path& first)
	{
		debug("removing read-only from " + first.string());
		check(first);

		if (!dry)
		{
			if (fs::is_regular_file(first))
				do_remove_readonly(first);

			for (auto&& p : fs::recursive_directory_iterator(first))
			{
				if (fs::is_regular_file(p))
					do_remove_readonly(p);
			}
		}
	}

	static void copy_file_to_dir(const fs::path& file, const fs::path& dir)
	{
		info(file.string() + " -> " + dir.string());

		check(file);
		check(dir);

		if (!fs::exists(file) || !fs::is_regular_file(file))
			bail_out("can't copy " + file.string() + ", not a file");

		if (fs::exists(dir) && !fs::is_directory(dir))
			bail_out("can't copy to " + dir.string() + ", not a directory");

		if (!dry)
			do_copy_file_to_dir(file, dir);
	}

	static void run(const std::string& cmd, const fs::path& cwd={})
	{
		debug("> " + cmd);

		if (!dry)
		{
			const int r = do_run(cmd, cwd);
			if (r != 0)
				bail_out("command returned " + std::to_string(r));
		}
	}

private:
	static void do_create_directories(const fs::path& p)
	{
		std::error_code ec;
		fs::create_directories(p, ec);

		if (ec)
			bail_out("can't create " + p.string(), ec);
	}

	static void do_delete_directory(const fs::path& p)
	{
		if (!fs::exists(p))
			return;

		std::error_code ec;
		fs::remove_all(p, ec);

		if (ec)
		{
			if (ec.value() == ERROR_ACCESS_DENIED)
			{
				remove_readonly(p);
				fs::remove_all(p, ec);

				if (!ec)
					return;
			}

			bail_out("failed to delete " + p.string(), ec);
		}
	}

	static void do_delete_file(const fs::path& p)
	{
		std::error_code ec;
		fs::remove(p, ec);

		if (ec)
			bail_out("can't delete " + p.string(), ec);
	}

	static void do_copy_file_to_dir(const fs::path& f, const fs::path& d)
	{
		create_directories(d);

		std::error_code ec;
		fs::copy_file(
			f, d / f.filename(),
			fs::copy_options::overwrite_existing, ec);

		if (ec)
			bail_out("can't copy " + f.string() + " to " + d.string(), ec);
	}

	static void do_remove_readonly(const fs::path& p)
	{
		std::error_code ec;
		fs::permissions(p, fs::perms::owner_write, fs::perm_options::add, ec);

		if (ec)
			bail_out("can't remove read-only flag on " + p.string(), ec);
	}

	static int do_run(const std::string& cmd, const fs::path& cwd)
	{
		if (cwd.empty())
		{
			return std::system(cmd.c_str());
		}
		else
		{
			create_directories(cwd);
			return std::system(("cd \"" + cwd.string() + "\" && " + cmd).c_str());
		}
	}

	static void check(const fs::path& p)
	{
		if (p.empty())
			bail_out("path is empty");

		if (p.native().starts_with(paths::prefix().native()))
			return;

		if (p.native().starts_with(paths::temp_dir().native()))
			return;

		bail_out("path " + p.string() + " is outside prefix");
	}
};


class url
{
public:
	url(const char* p)
		: s_(p)
	{
	}

	url(std::string s={})
		: s_(std::move(s))
	{
	}

	const char* c_str() const
	{
		return s_.c_str();
	}

	const std::string& string() const
	{
		return s_;
	}

	std::string file() const
	{
		const auto pos = s_.find_last_of("/");

		if (pos == std::string::npos)
			return s_;
		else
			return s_.substr(pos + 1);
	}

private:
	std::string s_;
};


class downloader
{
public:
	downloader(fs::path where, url u) :
		where_(std::move(where)), url_(std::move(u)),
		file_(nullptr), bailed_(false)
	{
		path_ = where_ / url_.file();
	}

	void start()
	{
		debug("downloading " + url_.string());

		op::create_directories(where_);

		if (fs::exists(path_))
		{
			debug("download " + path_.string() + " already exists");
			return;
		}

		thread_ = std::thread([&]
		{
			try
			{
				run();
			}
			catch(bailed)
			{
				bailed_ =true;
			}
		});
	}

	void join()
	{
		if (thread_.joinable())
			thread_.join();

		if (bailed_)
			throw bailed();
	}

	fs::path file() const
	{
		return path_;
	}

private:
	fs::path where_;
	url url_;
	fs::path path_;
	std::FILE* file_;
	std::thread thread_;
	bool bailed_;

	void run()
	{
		auto* c = curl_easy_init();

		curl_easy_setopt(c, CURLOPT_URL, url_.c_str());
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_write_static);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1l);


		const auto r = curl_easy_perform(c);

		if (r == 0)
		{
			long h = 0;
			curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &h);

			if (h != 200)
				bail_out(url_.string() + " http code: " + std::to_string(h));
		}
		else
		{
			bail_out(url_.string() + " curl: " + curl_easy_strerror(r));
		}

		curl_easy_cleanup(c);

		if (file_)
			std::fclose(file_);
	}

	static size_t on_write_static(
		char* ptr, size_t size, size_t nmemb, void* user) noexcept
	{
		auto* self = static_cast<downloader*>(user);
		self->on_write(ptr, size * nmemb);
		return size * nmemb;
	}

	void on_write(char* ptr, std::size_t n) noexcept
	{
		if (!file_)
			file_ = _wfopen(path_.native().c_str(), L"wb");

		std::fwrite(ptr, n, 1, file_);
	}
};


fs::path download(const url& u)
{
	downloader d(paths::cache(), u);

	d.start();
	d.join();

	return d.file();
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

void expand(const fs::path& file, const fs::path& where)
{
	if (fs::exists(where))
	{
		debug(
			"not expanding " + file.string() + ", " +
			where.string() + " already exists");

		return;
	}

	const auto sevenz = find_sevenz();

	op::delete_directory(where);
	op::create_directories(where);

	if (file.string().ends_with(".tar.gz"))
	{
		op::run(
			sevenz.string() + " x -so \"" + file.string() + "\" | " +
			sevenz.string() + " x -spe -si -ttar -o\"" + where.string() + "\" " + redir_nul());
	}
	else
	{
		op::run(
			sevenz.string() + " x -spe -bd -bb0 -o\"" + where.string() + "\" "
			"\"" + file.string() + "\" " + redir_nul());
	}
}


void cmake(const fs::path& build, const fs::path& prefix, const std::string& generator)
{
	std::string cmd = "cmake -G \"" + generator + "\"";

	if (!prefix.empty())
		cmd += " -DCMAKE_INSTALL_PREFIX=\"" + prefix.string() + "\"";

	cmd += " ..",
	op::run(cmd, build);
}

fs::path cmake_for_nmake(const fs::path& root, const fs::path& prefix={})
{
	const auto build = root / "build";
	const std::string g = "NMake Makefiles";
	cmake(build, prefix, g);
	return build;
}

fs::path cmake_for_vs(const fs::path& root, const fs::path& prefix={})
{
	const auto build = root / "vsbuild";
	const std::string g = "Visual Studio " + versions::vs_version() + " " + versions::vs_year();
	cmake(build, prefix, g);
	return build;
}

void nmake(const fs::path& dir)
{
	op::run("nmake", dir);
	op::run("nmake install", dir);
}


class sevenz
{
public:
	void run()
	{
		info("7z");

		const auto nodots = replace_all(versions::sevenzip(), ".", "");
		const auto file = download("https://www.7-zip.org/a/7z" + nodots + "-src.7z");

		const auto dir = paths::build() / ("7zip-" + versions::sevenzip());
		expand(file, dir);

		const fs::path src = dir / "CPP" / "7zip" / "Bundles" / "Format7zF";

		op::run(std::string("nmake ") +
			(verbose ? "" : "/C /S ") +
			"/NOLOGO CPU=x64 NEW_COMPILER=1 "
			"MY_STATIC_LINK=1 NO_BUFFEROVERFLOWU=1 ",
			src);

		op::copy_file_to_dir(src / "x64/7z.dll", paths::install_dlls());
	}
};


class zlib
{
public:
	void run()
	{
		info("zlib");

		const auto file = download("http://zlib.net/zlib-" + versions::zlib() + ".tar.gz");

		const auto dir = paths::build() / ("zlib-" + versions::zlib());
		expand(file, dir);

		const auto build = cmake_for_nmake(dir, dir);
		nmake(build);

		op::copy_file_to_dir(build / "zconf.h", dir);
	}
};



fs::path find_vcvars()
{
	const std::vector<std::string> editions =
	{
		"Preview", "Enterprise", "Professional", "Community"
	};

	for (auto&& edition : editions)
	{
		const auto p =
			paths::program_files_x86() /
			"Microsoft Visual Studio" /
			versions::vs_year() /
			edition / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat";

		if (fs::exists(p))
		{
			debug("found " + p.string());
			return p;
		}
	}

	bail_out("couldn't find visual studio");
}

void vcvars()
{
	const fs::path tmp = paths::temp_file();

	const std::string cmd =
		"\"" + find_vcvars().string() + "\" amd64 " + redir_nul() + " && "
		"set > " + tmp.string();

	op::run(cmd);

	std::stringstream ss(read_text_file(tmp));
	op::delete_file(tmp);

	for (;;)
	{
		std::string line;
		std::getline(ss, line);
		if (!ss)
			break;

		const auto sep = line.find('=');

		if (sep == std::string::npos)
			continue;

		const std::string name = line.substr(0, sep);
		const std::string value = line.substr(sep + 1);

		debug("setting env " + name);
		SetEnvironmentVariableA(name.c_str(), nullptr);
		SetEnvironmentVariableA(name.c_str(), value.c_str());
	}
}

int run()
{
	try
	{
		vcvars();

		//sevenz().run();
		zlib().run();

		return 0;
	}
	catch(bailed)
	{
		error("bailing out");
		return 1;
	}
}

} // namespace


int main()
{
	int r = builder::run();
	std::cin.get();
	return r;
}

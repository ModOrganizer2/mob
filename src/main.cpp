#include "pch.h"
#include "conf.h"
#include "net.h"
#include "op.h"
#include "tools.h"
#include "utility.h"

namespace builder
{

class interrupted {};

class task;
std::vector<std::unique_ptr<task>> g_tasks;

template <class T>
bool add_task()
{
	g_tasks.push_back(std::make_unique<T>());
	return true;
}


class task
{
public:
	task(const task&) = delete;
	task& operator=(const task&) = delete;

	virtual ~task()
	{
		try
		{
			join();
		}
		catch(bailed)
		{
			// ignore
		}
	}

	static void interrupt_all()
	{
		std::scoped_lock lock(interrupt_mutex_);
		for (auto&& t : g_tasks)
			t->interrupt();
	}


	const std::string& name() const
	{
		return name_;
	}

	void run()
	{
		info(name_);

		thread_ = std::thread([&]
		{
			try
			{
				check_interrupted();
				fetch();
				check_interrupted();
				build_and_install();
			}
			catch(bailed e)
			{
				error(name_ + " bailed out, interrupting all tasks");
				interrupt_all();
			}
			catch(interrupted)
			{
				return;
			}
			catch(std::exception& e)
			{
				error(name_ + " uncaught exception: " + e.what());
				interrupt_all();
			}
		});
	}

	void interrupt()
	{
		std::scoped_lock lock(tool_mutex_);

		interrupted_ = true;
		if (tool_)
			tool_->interrupt();
	}

	void join()
	{
		if (thread_.joinable())
			thread_.join();
	}

	void fetch()
	{
		do_fetch();
		run_tool<patcher>(paths::patches() / name_, source_path());
	}

	void build_and_install()
	{
		do_build_and_install();
	}

protected:
	task(std::string name)
		: name_(std::move(name)), interrupted_(false)
	{
	}

	void check_interrupted()
	{
		if (interrupted_)
			throw interrupted();
	}

	virtual void do_fetch() {};
	virtual void do_build_and_install() {};
	virtual fs::path source_path() const = 0;

	template <class Tool, class... Args>
	std::unique_ptr<Tool> run_tool(Args&&... args)
	{
		Tool* p = nullptr;

		{
			std::scoped_lock lock(tool_mutex_);
			p = new Tool(std::forward<Args>(args)...);
			tool_.reset(p);
		}

		check_interrupted();
		p->run();
		check_interrupted();

		{
			std::scoped_lock lock(tool_mutex_);
			tool_.release();
			return std::unique_ptr<Tool>(p);
		}
	}

private:
	std::string name_;
	std::thread thread_;
	std::atomic<bool> interrupted_;

	std::unique_ptr<tool> tool_;
	std::mutex tool_mutex_;

	static std::mutex interrupt_mutex_;
};

std::mutex task::interrupt_mutex_;


class sevenz : public task
{
public:
	sevenz()
		: task("7z")
	{
	}

protected:
	fs::path source_path() const override
	{
		return paths::build() / ("7zip-" + versions::sevenzip());
	}

	void do_fetch() override
	{
		const auto nodots = replace_all(versions::sevenzip(), ".", "");

		const auto file = run_tool<downloader>(
			"https://www.7-zip.org/a/7z" + nodots + "-src.7z")->file();

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		const fs::path src =
			source_path() / "CPP" / "7zip" / "Bundles" / "Format7zF";

		run_tool<jom>(src, "",
			"/NOLOGO CPU=x64 NEW_COMPILER=1 "
			"MY_STATIC_LINK=1 NO_BUFFEROVERFLOWU=1");

		op::copy_file_to_dir_if_better(src / "x64/7z.dll", paths::install_dlls());
	}
};


class zlib : public task
{
public:
	zlib()
		: task("zlib")
	{
	}

protected:
	fs::path source_path() const override
	{
		return paths::build() / ("zlib-" + versions::zlib());
	}

	void do_fetch() override
	{
		const auto file = run_tool<downloader>(
			"http://zlib.net/zlib-" + versions::zlib() + ".tar.gz")->file();

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		run_tool<cmake_for_nmake>(source_path(), "", source_path());
		run_tool<jom>(source_path() / cmake_for_nmake::build_path(), "install", "");

		op::copy_file_to_dir_if_better(
			source_path() / cmake_for_nmake::build_path() / "zconf.h",
			source_path());
	}
};


class boost : public task
{
public:
	boost()
		: task("boost")
	{
	}

protected:
	fs::path source_path() const override
	{
		const auto underscores = replace_all(versions::boost(), ".", "_");
		return paths::build() / ("boost_" + underscores);
	}

	void do_fetch() override
	{
		if (prebuilt::boost())
			fetch_prebuilt();
		else
			fetch_from_source();
	}

	void do_build_and_install() override
	{
		if (prebuilt::boost())
			build_and_install_prebuilt();
		else
			build_and_install_from_source();
	}

private:
	std::smatch parse_boost_version() const
	{
		// 1.72.0-b1-rc1
		// everything but 1.72 is optional
		std::regex re(R"((\d+)\.(\d+)(?:\.(\d+)(?:-(\w+)(?:-(\w+))?)?)?)");
		std::smatch m;

		if (!std::regex_match(versions::boost(), m, re))
			bail_out("bad boost version '" + versions::boost() + "'");

		return m;
	}

	std::string source_download_filename() const
	{
		return boost_version_all_underscores() + ".zip";
	}

	void fetch_prebuilt()
	{
		const auto underscores = replace_all(versions::boost(), ".", "_");

		const auto file = run_tool<downloader>(
			"https://github.com/ModOrganizer2/modorganizer-umbrella/"
			"releases/download/1.1/boost_prebuilt_" + underscores + ".7z")
				->file();

		run_tool<decompresser>(file, source_path());
	}

	void build_and_install_prebuilt()
	{
		op::copy_file_to_dir_if_better(
			lib_path() / python_dll(), paths::install_bin());
	}

	void fetch_from_source()
	{
		const auto file = run_tool<downloader>(
			"https://dl.bintray.com/boostorg/release/" +
			boost_version_no_tags() + "/source/" +
			boost_version_all_underscores() + ".zip")->file();

		run_tool<decompresser>(file, source_path());
	}

	void build_and_install_from_source()
	{
	}

	fs::path lib_path() const
	{
		const std::string lib = "lib64-msvc-" + versions::boost_vs();
		return source_path() / lib / "lib";
	}

	std::string python_dll() const
	{
		std::ostringstream oss;

		// builds something like boost_python38-vc142-mt-x64-1_72.dll

		// boost_python38-
		oss << "boost_python" << python_version_no_patch() + "-";

		// vc142-
		oss << "vc" + replace_all(versions::boost_vs(), ".", "") << "-";

		// mt-x64-1_72
		oss << "mt-x64-" << boost_version_no_patch_underscores();

		oss << ".dll";

		return oss.str();
	}

	std::string python_version_no_patch() const
	{
		// matches 3.8 and 3.8.1
		std::regex re(R"((\d+)\.(\d+)(?:\.(\d+))?)");

		std::smatch m;
		if (!std::regex_match(versions::python(), m, re))
			bail_out("bad python version '" + versions::python() + "'");

		// 38
		return m[1].str() + m[2].str();
	}

	std::string boost_version_no_patch_underscores() const
	{
		const auto m = parse_boost_version();

		// 1_72
		return m[1].str() + "_" + m[2].str();
	}

	std::string boost_version_no_tags() const
	{
		const auto m = parse_boost_version();

		// 1.72.1
		std::string s = m[1].str() + "." + m[2].str();

		if (m.size() > 3)
			s += "." + m[3].str();

		return s;
	}

	std::string boost_version_all_underscores() const
	{
		const auto m = parse_boost_version();

		// boost_1_72_0_b1_rc1
		std::string s = "boost_" + m[1].str() + "_" + m[2].str();

		if (m.size() > 3)
			s += "_" + m[3].str();

		if (m.size() > 4)
			s += "_" + m[4].str();

		if (m.size() > 5)
			s += "_" + m[5].str();

		return s;
	}
};


class fmt : public task
{
public:
	fmt()
		: task("fmt")
	{
	}

protected:
	fs::path source_path() const override
	{
		return paths::build() / ("fmt-" + versions::fmt());
	}

	void do_fetch() override
	{
		const auto file = run_tool<downloader>(
			"https://github.com/fmtlib/fmt/releases/download/" +
			versions::fmt() + "/fmt-" + versions::fmt() + ".zip")->file();

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		run_tool<cmake_for_nmake>(source_path(), "-DFMT_TEST=OFF -DFMT_DOC=OFF");
		run_tool<jom>(source_path() / cmake_for_nmake::build_path(), "", "");
	}
};


class gtest : public task
{
public:
	gtest()
		: task("gtest")
	{
	}

protected:
	fs::path source_path() const override
	{
		return paths::build() / "googletest";
	}

	void do_fetch() override
	{
		run_tool<git_clone>("google", "googletest", versions::gtest(), source_path());
	}

	void do_build_and_install() override
	{
		run_tool<cmake_for_nmake>(source_path());
		run_tool<jom>(source_path() / cmake_for_nmake::build_path(), "", "");
	}
};


class libbsarch : public task
{
public:
	libbsarch()
		: task("libbsarch")
	{
	}

protected:
	std::string dir_name() const
	{
		return "libbsarch-" + versions::libbsarch() + "-release-x64";
	}

	fs::path source_path() const override
	{
		return paths::build() / dir_name();
	}

	void do_fetch() override
	{
		const auto file = run_tool<downloader>(
			"https://github.com/ModOrganizer2/libbsarch/releases/download/" +
			versions::libbsarch() + "/" + dir_name() + ".7z")->file();

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		op::copy_file_to_dir_if_better(
			source_path() / "libbsarch.dll",
			paths::install_dlls());
	}
};


class libloot : public task
{
public:
	libloot()
		: task("libloot")
	{
	}

protected:
	std::string dir_name() const
	{
		// libloot-0.15.1-0-gf725dd7_0.15.1-win64.7z, yeah
		return
			"libloot-" +
			versions::libloot() + "-" +
			"0-" +
			versions::libloot_hash() + "_" + versions::libloot() + "-" +
			"win64";
	}

	fs::path source_path() const override
	{
		return paths::build() / dir_name();
	}

	void do_fetch() override
	{
		const auto file = run_tool<downloader>(
			"https://github.com/loot/libloot/releases/download/" +
			versions::libloot() + "/" + dir_name() + ".7z")->file();

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		op::copy_file_to_dir_if_better(
			source_path() / "loot.dll",
			paths::install_loot());
	}
};


class libffi : public task
{
public:
	libffi()
		: task("libffi")
	{
	}

protected:
	fs::path source_path() const override
	{
		return paths::build() / "libffi";
	}

	void do_fetch() override
	{
		run_tool<git_clone>(
			"python", "cpython-bin-deps", "libffi",
			source_path());
	}
};


class openssl : public task
{
public:
	openssl()
		: task("openssl")
	{
	}

protected:
	fs::path source_path() const override
	{
		return paths::build() / ("openssl-" + versions::openssl());
	}

	fs::path build_path() const
	{
		return source_path() / "build";
	}

	void do_fetch() override
	{
		const std::string filename = "openssl-" + versions::openssl() + ".tar.gz";
		const auto file = run_tool<downloader>(
			"https://www.openssl.org/source/" + filename)->file();

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		if (fs::exists(source_path() / "makefile"))
			debug("openssl already configured");
		else
			configure();

		install_engines();
		info("openssl built successfully");

		copy_files();
	}

private:
	void configure()
	{
		run_tool<process_runner>(
			"perl",
			"\"" + third_party::perl().string() + "\" " +
			"Configure "
			"--openssldir=\"" + build_path().string() + "\" "
			"--prefix=\"" + build_path().string() + "\" "
			"-FS -MP1 "
			"VC-WIN64A",
			source_path());
	}

	void install_engines()
	{
		for (int tries=0; tries<3; ++tries)
		{
			const auto tool = run_tool<jom>(
				source_path(), "install_engines", "", jom::accept_failure);

			if (tool->exit_code() == 0)
				return;
		}

		run_tool<jom>(source_path(), "install_engines", "", jom::single_job);
	}

	void copy_files()
	{
		copy_dlls_to(paths::install_bin());
		copy_dlls_to(paths::install_dlls());
		copy_pdbs_to(paths::install_pdbs());
	}

	void copy_dlls_to(const fs::path& dir)
	{
		for (auto&& name : output_names())
		{
			op::copy_file_to_dir_if_better(
				build_path() / "bin" / (name + ".dll"), dir);
		}
	}

	void copy_pdbs_to(const fs::path& dir)
	{
		for (auto&& name : output_names())
		{
			op::copy_file_to_dir_if_better(
				build_path() / "bin" / (name + ".pdb"), dir);
		}
	}

	std::vector<std::string> output_names() const
	{
		return
		{
			"libcrypto-" + version_no_minor_underscores() + "-x64",
			"libssl-" + version_no_minor_underscores() + "-x64"
		};
	}

	std::smatch parse_version() const
	{
		// 1.1.1d
		// everything but 1 is optional
		std::regex re(R"((\d+)(?:\.(\d+)(?:\.(\d+)([a-zA-Z]+)?)?)?)");
		std::smatch m;

		if (!std::regex_match(versions::openssl(), m, re))
			bail_out("bad openssl version '" + versions::openssl() + "'");

		return m;
	}

	std::string version_no_tags() const
	{
		auto m = parse_version();

		// up to 4 so the tag is skipped if present
		const std::size_t count = std::min<std::size_t>(m.size(), 4);

		std::string s;
		for (std::size_t i=1; i<count; ++i)
		{
			if (!s.empty())
				s += ".";

			s += m[i].str();
		}

		return s;
	}

	std::string version_no_minor_underscores() const
	{
		auto m = parse_version();

		if (m.size() == 2)
			return m[1].str();
		else
			return m[1].str() + "_" + m[2].str();
	}
};


class python : public task
{
public:
	python()
		: task("python")
	{
	}

protected:
	std::string dir_name() const
	{
		// libloot-0.15.1-0-gf725dd7_0.15.1-win64.7z, yeah
		return
			"libloot-" +
			versions::libloot() + "-" +
			"0-" +
			versions::libloot_hash() + "_" + versions::libloot() + "-" +
			"win64";
	}

	fs::path source_path() const override
	{
		return paths::build() / dir_name();
	}

	void do_fetch() override
	{
		const auto file = run_tool<downloader>(
			"https://github.com/loot/libloot/releases/download/" +
			versions::libloot() + "/" + dir_name() + ".7z")->file();

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		op::copy_file_to_dir_if_better(
			source_path() / "loot.dll",
			paths::install_loot());
	}
};


template <class Tool, class... Args>
class dummy : public task
{
public:
	dummy(Args&&... args)
		: task("dummy"), args_(std::forward<Args>(args)...)
	{
	}


protected:
	void do_fetch() override
	{
		std::apply([&](auto&&... args){ run_tool<Tool>(args...); }, args_);
	}

private:
	std::tuple<Args...> args_;
};


template <class Tool, class... Args>
std::unique_ptr<dummy<Tool, Args...>> make_dummy(Args&&... args)
{
	using Dummy = dummy<Tool, Args...>;
	return std::unique_ptr<Dummy>(new Dummy(std::forward<Args>(args)...));
}


BOOL WINAPI signal_handler(DWORD) noexcept
{
	info("caught sigint");
	task::interrupt_all();
	return TRUE;
}


int run(int argc, char** argv)
{
	try
	{
		::SetConsoleCtrlHandler(signal_handler, TRUE);

		vcvars();
		prepend_to_path(find_third_party_directory() / "bin");

		//g_tasks.push_back(std::make_unique<sevenz>());
		//g_tasks.push_back(std::make_unique<zlib>());
		//g_tasks.push_back(std::make_unique<boost>());
		//g_tasks.push_back(std::make_unique<fmt>());
		//g_tasks.push_back(std::make_unique<gtest>());
		//g_tasks.push_back(std::make_unique<libbsarch>());
		//g_tasks.push_back(std::make_unique<libloot>());
		//g_tasks.push_back(std::make_unique<python>());
		//g_tasks.push_back(std::make_unique<libffi>());
		g_tasks.push_back(std::make_unique<openssl>());

		if (argc > 1)
		{
			for (auto&& t : g_tasks)
			{
				if (t->name() == argv[1])
				{
					t->run();
					t->join();
					return 0;
				}
			}

			std::cerr << std::string("task ") + argv[1] + " not found\n";
			std::cerr << "valid tasks:\n";

			for (auto&& t : g_tasks)
				std::cerr << " - " << t->name() << "\n";

			return 1;
		}


		for (auto&& t : g_tasks)
			t->run();

		for (auto&& t : g_tasks)
			t->join();

		return 0;
	}
	catch(bailed)
	{
		error("bailing out");
		return 1;
	}
}

} // namespace


int main(int argc, char** argv)
{
	int r = builder::run(argc, argv);
	builder::dump_logs();
	//std::cin.get();
	return r;
}

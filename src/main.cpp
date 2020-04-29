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

	virtual fs::path get_source_path() const = 0;


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

		run_tool(patcher()
			.task(name_)
			.root(get_source_path()));
	}

	void build_and_install()
	{
		do_build_and_install();
	}

protected:
	task(std::string name)
		: name_(std::move(name)), interrupted_(false), tool_(nullptr)
	{
	}

	void check_interrupted()
	{
		if (interrupted_)
			throw interrupted();
	}

	virtual void do_fetch() {};
	virtual void do_build_and_install() {};

	template <class Tool>
	auto run_tool(Tool&& t)
	{
		{
			tool_ = &t;
			std::scoped_lock lock(tool_mutex_);
		}

		check_interrupted();
		t.run();
		check_interrupted();

		{
			std::scoped_lock lock(tool_mutex_);
			tool_ = nullptr;
		}

		return t.result();
	}

private:
	std::string name_;
	std::thread thread_;
	std::atomic<bool> interrupted_;

	tool* tool_;
	std::mutex tool_mutex_;

	static std::mutex interrupt_mutex_;
};

std::mutex task::interrupt_mutex_;


template <class Task>
class basic_task : public task
{
public:
	using task::task;

	fs::path get_source_path() const override
	{
		return Task::source_path();
	}
};


class sevenz : public basic_task<sevenz>
{
public:
	sevenz()
		: basic_task("7z")
	{
	}

	static fs::path source_path()
	{
		return paths::build() / ("7zip-" + versions::sevenzip());
	}

protected:
	void do_fetch() override
	{
		const auto file = run_tool(downloader(source_url()));

		run_tool(decompresser()
			.file(file)
			.output(source_path()));
	}

	void do_build_and_install() override
	{
		run_tool(jom()
			.path(module_to_build())
			.def("CPU=x64")
			.def("NEW_COMPILER=1")
			.def("MY_STATIC_LINK=1")
			.def("NO_BUFFEROVERFLOWU=1"));

		op::copy_file_to_dir_if_better(
			module_to_build() / "x64/7z.dll",
			paths::install_dlls());
	}

private:
	url source_url() const
	{
		return "https://www.7-zip.org/a/7z" + version_for_url() + "-src.7z";
	}

	fs::path module_to_build() const
	{
		return source_path() / "CPP" / "7zip" / "Bundles" / "Format7zF";
	}

	std::string version_for_url() const
	{
		return replace_all(versions::sevenzip(), ".", "");
	}
};


class zlib : public basic_task<zlib>
{
public:
	zlib()
		: basic_task("zlib")
	{
	}

	static fs::path source_path()
	{
		return paths::build() / ("zlib-" + versions::zlib());
	}

protected:
	void do_fetch() override
	{
		const auto file = run_tool(downloader(source_url()));

		run_tool(decompresser()
			.file(file)
			.output(source_path()));
	}

	void do_build_and_install() override
	{
		const auto build_path = run_tool(cmake()
			.generator(cmake::nmake)
			.root(source_path())
			.prefix(source_path()));

		run_tool(jom()
			.path(build_path)
			.target("install"));

		op::copy_file_to_dir_if_better(
			build_path / "zconf.h",
			source_path());
	}

private:
	url source_url() const
	{
		return "https://zlib.net/zlib-" + versions::zlib() + ".tar.gz";
	}
};


class boost : public basic_task<boost>
{
public:
	boost()
		: basic_task("boost")
	{
	}

	static fs::path source_path()
	{
		const auto underscores = replace_all(versions::boost(), ".", "_");
		return paths::build() / ("boost_" + underscores);
	}

protected:
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
		const auto file = run_tool(downloader(prebuilt_url()));

		run_tool(decompresser()
			.file(file)
			.output(source_path()));
	}

	void build_and_install_prebuilt()
	{
		op::copy_file_to_dir_if_better(
			lib_path() / python_dll(), paths::install_bin());
	}

	void fetch_from_source()
	{
		const auto file = run_tool(downloader(source_url()));

		run_tool(decompresser()
			.file(file)
			.output(source_path()));
	}

	void build_and_install_from_source()
	{
	}


	url prebuilt_url() const
	{
		const auto underscores = replace_all(versions::boost(), ".", "_");

		return
			"https://github.com/ModOrganizer2/modorganizer-umbrella/"
			"releases/download/1.1/boost_prebuilt_" + underscores + ".7z";
	}

	url source_url() const
	{
		return
			"https://dl.bintray.com/boostorg/release/" +
			boost_version_no_tags() + "/source/" +
			boost_version_all_underscores() + ".zip";
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


class fmt : public basic_task<fmt>
{
public:
	fmt()
		: basic_task("fmt")
	{
	}

	static fs::path source_path()
	{
		return paths::build() / ("fmt-" + versions::fmt());
	}

protected:
	void do_fetch() override
	{
		const auto file = run_tool(downloader(source_url()));

		run_tool(decompresser()
			.file(file)
			.output(source_path()));
	}

	void do_build_and_install() override
	{
		const auto build_path = run_tool(cmake()
			.generator(cmake::nmake)
			.root(source_path())
			.def("FMT_TEST=OFF")
			.def("FMT_DOC=OFF"));

		run_tool(jom().path(build_path));
	}

private:
	url source_url() const
	{
		return
			"https://github.com/fmtlib/fmt/releases/download/" +
			versions::fmt() + "/fmt-" + versions::fmt() + ".zip";
	}
};


class gtest : public basic_task<gtest>
{
public:
	gtest()
		: basic_task("gtest")
	{
	}

	static fs::path source_path()
	{
		return paths::build() / "googletest";
	}

protected:
	void do_fetch() override
	{
		run_tool(git_clone()
			.org("google")
			.repo("googletest")
			.branch(versions::gtest())
			.output(source_path()));
	}

	void do_build_and_install() override
	{
		const auto build_path = run_tool(cmake()
			.generator(cmake::nmake)
			.root(source_path()));

		run_tool(jom()
			.path(build_path));
	}
};


class libbsarch : public basic_task<libbsarch>
{
public:
	libbsarch()
		: basic_task("libbsarch")
	{
	}

	static fs::path source_path()
	{
		return paths::build() / dir_name();
	}

protected:
	static std::string dir_name()
	{
		return "libbsarch-" + versions::libbsarch() + "-release-x64";
	}

	void do_fetch() override
	{
		const auto file = run_tool(downloader(source_url()));

		run_tool(decompresser()
			.file(file)
			.output(source_path()));
	}

	void do_build_and_install() override
	{
		op::copy_file_to_dir_if_better(
			source_path() / "libbsarch.dll",
			paths::install_dlls());
	}

private:
	url source_url() const
	{
		return
			"https://github.com/ModOrganizer2/libbsarch/releases/download/" +
			versions::libbsarch() + "/" + dir_name() + ".7z";
	}
};


class libloot : public basic_task<libloot>
{
public:
	libloot()
		: basic_task("libloot")
	{
	}

	static fs::path source_path()
	{
		return paths::build() / dir_name();
	}

protected:
	static std::string dir_name()
	{
		// libloot-0.15.1-0-gf725dd7_0.15.1-win64.7z, yeah
		return
			"libloot-" +
			versions::libloot() + "-" +
			"0-" +
			versions::libloot_hash() + "_" + versions::libloot() + "-" +
			"win64";
	}

	void do_fetch() override
	{
		const auto file = run_tool(downloader(source_url()));

		run_tool(decompresser()
			.file(file)
			.output(source_path()));
	}

	void do_build_and_install() override
	{
		op::copy_file_to_dir_if_better(
			source_path() / "loot.dll",
			paths::install_loot());
	}

private:
	url source_url() const
	{
		return
			"https://github.com/loot/libloot/releases/download/" +
			versions::libloot() + "/" + dir_name() + ".7z";
	}
};


class libffi : public basic_task<libffi>
{
public:
	libffi()
		: basic_task("libffi")
	{
	}

	static fs::path source_path()
	{
		return paths::build() / "libffi";
	}

	static fs::path include_path()
	{
		return source_path() / "amd64" / "include";
	}

	static fs::path lib_path()
	{
		return source_path() / "amd64";
	}

protected:
	void do_fetch() override
	{
		run_tool(git_clone()
			.org("python")
			.repo("cpython-bin-deps")
			.branch("libffi")
			.output(source_path()));
	}
};


class openssl : public basic_task<openssl>
{
public:
	openssl()
		: basic_task("openssl")
	{
	}

	static fs::path source_path()
	{
		return paths::build() / ("openssl-" + versions::openssl());
	}

	static fs::path include_path()
	{
		return source_path() / "include";
	}

protected:
	fs::path build_path() const
	{
		return source_path() / "build";
	}

	void do_fetch() override
	{
		const auto file = run_tool(downloader(source_url()));

		run_tool(decompresser()
			.file(file)
			.output(source_path()));
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
		run_tool(process_runner(third_party::perl(), cmd::stdout_is_verbose)
			.arg("Configure")
			.arg("--openssldir=", build_path())
			.arg("--prefix=", build_path())
			.arg("-FS")
			.arg("-MP1")
			.arg("VC-WIN64A")
			.cwd(source_path()));
	}

	void install_engines()
	{
		for (int tries=0; tries<3; ++tries)
		{
			int exit_code = run_tool(jom()
				.path(source_path())
				.target("install_engines")
				.flag(jom::accept_failure));

			if (exit_code == 0)
				return;
		}

		run_tool(jom()
			.path(source_path())
			.target("install_engines")
			.flag(jom::single_job));
	}

	void copy_files()
	{
		op::copy_file_to_dir_if_better(
			source_path() / "ms" / "applink.c",
			include_path());

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


	url source_url() const
	{
		return
			"https://www.openssl.org/source/"
			"openssl-" + versions::openssl() + ".tar.gz";
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


class bzip2 : public basic_task<bzip2>
{
public:
	bzip2()
		: basic_task("bzip2")
	{
	}

	static fs::path source_path()
	{
		return paths::build() / ("bzip2-" + versions::bzip2());
	}

protected:
	void do_fetch() override
	{
		const auto file = run_tool(downloader(source_url()));

		run_tool(decompresser()
			.file(file)
			.output(source_path()));
	}

private:
	url source_url() const
	{
		return
			"https://sourceforge.net/projects/bzip2/files/"
			"bzip2-" + versions::bzip2() + ".tar.gz/download";
	}
};


class python : public basic_task<python>
{
public:
	python()
		: basic_task("python")
	{
	}

	static fs::path source_path()
	{
		return paths::build() / ("python-" + versions::python());
	}

protected:
	void do_fetch() override
	{
		run_tool(git_clone()
			.org("python")
			.repo("cpython")
			.branch("v" + versions::python())
			.output(source_path()));

		if (fs::exists(source_path() / "PCBuild" / "UpgradeLog.htm"))
			debug("project already upgraded");
		else
			upgrade_project();
	}

	void do_build_and_install() override
	{
		run_tool(msbuild()
			.solution(solution_file())
			.projects({
				"python", "pythonw", "python3dll", "select", "pyexpat",
				"unicodedata", "_queue", "_bz2", "_ssl"})
			.parameters({
				"bz2Dir=" + bzip2::source_path().string(),
				"zlibDir=" + zlib::source_path().string(),
				"opensslIncludeDir=" + openssl::include_path().string(),
				"opensslOutDir=" + openssl::source_path().string(),
				"libffiIncludeDir=" + libffi::include_path().string(),
				"libffiOutDir=" + libffi::lib_path().string()}));

		if (fs::exists(build_path() / "_mob_packaged"))
		{
			debug("python already packaged");
		}
		else
		{
			const auto bat = source_path() / "python.bat";

			run_tool(process_runner(bat, cmd::stdout_is_verbose)
				.name("package python")
				.arg(fs::path("PC/layout"))
				.arg("--source", source_path())
				.arg("--build", build_path())
				.arg("--temp", (build_path() / "pythoncore_temp"))
				.arg("--copy", (build_path() / "pythoncore"))
				.arg("--preset-embed")
				.cwd(source_path()));

			op::touch(build_path() / "_mob_packaged");
		}

		op::copy_file_to_dir_if_better(
			source_path() / "PC" / "pyconfig.h",
			source_path() / "Include");

		op::copy_file_to_dir_if_better(
			build_path() / "*.lib",
			paths::install_libs());

		op::copy_file_to_dir_if_better(
			build_path() / "libffi*.dll",
			paths::install_bin());

		op::copy_file_to_dir_if_better(
			build_path() / ("python" + version_for_dll() + ".dll"),
			paths::install_bin());

		op::copy_file_to_dir_if_better(
			build_path() / ("python" + version_for_dll() + ".pdb"),
			paths::install_pdbs());
	}

private:
	void upgrade_project()
	{
		run_tool(process_runner(third_party::devenv(), cmd::stdout_is_verbose)
			.name("upgrade project")
			.arg(solution_file())
			.arg("/upgrade"));
	}

	fs::path solution_file() const
	{
		return source_path() / "PCBuild" / "pcbuild.sln";
	}

	fs::path build_path() const
	{
		return source_path() / "PCBuild" / "amd64";
	}

	std::smatch parse_version() const
	{
		// 3.8.1
		// .1 is optional
		std::regex re(R"((\d+)\.(\d+)(?:\.(\d+))?)");
		std::smatch m;

		if (!std::regex_match(versions::python(), m, re))
			bail_out("bad python version '" + versions::python() + "'");

		return m;
	}

	std::string version_for_dll() const
	{
		std::smatch m = parse_version();

		// 38
		return m[1].str() + m[2].str();
	}
};


template <class Tool, class... Args>
class dummy : public basic_task<dummy<Tool, Args...>>
{
public:
	dummy(Args&&... args)
		: basic_task("dummy"), args_(std::forward<Args>(args)...)
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


struct curl_init
{
	curl_init()
	{
		curl_global_init(CURL_GLOBAL_ALL );
	}

	~curl_init()
	{
		curl_global_cleanup();
	}
};


int run(int argc, char** argv)
{
	try
	{
		::SetConsoleCtrlHandler(signal_handler, TRUE);

		curl_init curl;
		vcvars();
		prepend_to_path(find_third_party_directory() / "bin");

		g_tasks.push_back(std::make_unique<sevenz>());
		g_tasks.push_back(std::make_unique<zlib>());
		g_tasks.push_back(std::make_unique<boost>());
		g_tasks.push_back(std::make_unique<fmt>());
		g_tasks.push_back(std::make_unique<gtest>());
		g_tasks.push_back(std::make_unique<libbsarch>());
		g_tasks.push_back(std::make_unique<libloot>());
		g_tasks.push_back(std::make_unique<openssl>());
		g_tasks.push_back(std::make_unique<libffi>());
		g_tasks.push_back(std::make_unique<bzip2>());
		g_tasks.push_back(std::make_unique<python>());

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
		{
			t->run();
			t->join();
		}

		//for (auto&& t : g_tasks)
		//	t->join();

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

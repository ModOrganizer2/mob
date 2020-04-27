#include "pch.h"
#include "conf.h"
#include "net.h"
#include "op.h"
#include "tools.h"
#include "utility.h"

namespace builder
{

class task;
std::vector<std::unique_ptr<task>> g_tasks;


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
				if (interrupted_)
					return;

				fetch();

				if (interrupted_)
					return;

				build_and_install();
			}
			catch(bailed e)
			{
				error(name_ + " bailed out, interrupting all tasks");
				interrupt_all();
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

	virtual void do_fetch() {};
	virtual void do_build_and_install() {};
	virtual fs::path source_path() const = 0;

	template <class Tool, class... Args>
	auto run_tool(Args&&... args)
	{
		Tool* p = nullptr;

		{
			std::scoped_lock lock(tool_mutex_);
			p = new Tool(std::forward<Args>(args)...);
			tool_.reset(p);
		}

		p->run();
		return p->result();
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
			"https://www.7-zip.org/a/7z" + nodots + "-src.7z");

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		const fs::path src =
			source_path() / "CPP" / "7zip" / "Bundles" / "Format7zF";

		run_tool<nmake>(src,
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
			"http://zlib.net/zlib-" + versions::zlib() + ".tar.gz");

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		run_tool<cmake_for_nmake>(source_path(), "", source_path());
		run_tool<nmake_install>(source_path() / cmake_for_nmake::build_path());

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
		const auto underscores = replace_all(versions::boost(), ".", "_");

		const auto file = run_tool<downloader>(
			"https://github.com/ModOrganizer2/modorganizer-umbrella/"
			"releases/download/1.1/boost_prebuilt_" + underscores + ".7z");

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		op::copy_file_to_dir_if_better(
			lib_path() / python_dll(), paths::install_bin());
	}

private:
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
		oss << "mt-x64-" << boost_version_no_patch();

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

	std::string boost_version_no_patch() const
	{
		// matches 1.71 or 1.71.0
		std::regex re(R"((\d+)\.(\d+)(?:\.(\d+))?)");

		std::smatch m;
		if (!std::regex_match(versions::boost(), m, re))
			bail_out("bad boost version '" + versions::boost() + "'");

		// 1_71
		return m[1].str() + "_" + m[2].str();
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
			versions::fmt() + "/fmt-" + versions::fmt() + ".zip");

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		run_tool<cmake_for_nmake>(source_path(), "-DFMT_TEST=OFF -DFMT_DOC=OFF");
		run_tool<nmake>(source_path() / cmake_for_nmake::build_path());
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
		run_tool<nmake>(source_path() / cmake_for_nmake::build_path());
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
	fs::path source_path() const override
	{
		return paths::build() / ("libbsarch-" + versions::libbsarch() + "-release-x64");
	}

	void do_fetch() override
	{
		const auto file = run_tool<downloader>(
			"https://github.com/ModOrganizer2/libbsarch/releases/download/" +
			versions::libbsarch() + "/" +
			"libbsarch-" + versions::libbsarch() + "-release-x64.7z");

		run_tool<decompresser>(file, source_path());
	}

	void do_build_and_install() override
	{
		op::copy_file_to_dir_if_better(
			source_path() / "libbsarch.dll",
			paths::install_dlls());
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

		g_tasks.push_back(std::make_unique<sevenz>());
		g_tasks.push_back(std::make_unique<zlib>());
		g_tasks.push_back(std::make_unique<boost>());
		g_tasks.push_back(std::make_unique<fmt>());
		g_tasks.push_back(std::make_unique<gtest>());
		g_tasks.push_back(std::make_unique<libbsarch>());

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

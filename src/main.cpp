#include "pch.h"
#include "conf.h"
#include "net.h"
#include "op.h"
#include "tools.h"
#include "utility.h"

namespace builder
{

class task
{
public:
	task(std::string name)
		: name_(std::move(name))
	{
	}

	virtual ~task() = default;

	void run()
	{
		info(name_);
		fetch();
		build();
		install();
	}

	void fetch()
	{
		do_fetch();
	}

	void build()
	{
		do_build();
	}

	void install()
	{
		do_install();
	}

protected:
	virtual void do_fetch() = 0;
	virtual void do_build() = 0;
	virtual void do_install() {};

private:
	std::string name_;
};


class sevenz : public task
{
public:
	sevenz()
		: task("7z")
	{
	}

protected:
	void do_fetch() override
	{
		const auto nodots = replace_all(versions::sevenzip(), ".", "");
		const auto file = download("https://www.7-zip.org/a/7z" + nodots + "-src.7z");
		decompress(file, src_path());
	}

	void do_build()
	{
		const fs::path src =
			src_path() / "CPP" / "7zip" / "Bundles" / "Format7zF";

		nmake(src,
			"/NOLOGO CPU=x64 NEW_COMPILER=1 "
			"MY_STATIC_LINK=1 NO_BUFFEROVERFLOWU=1");

		op::copy_file_to_dir(src / "x64/7z.dll", paths::install_dlls());
	}

private:
	fs::path src_path() const
	{
		return paths::build() / ("7zip-" + versions::sevenzip());
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
	void do_fetch()
	{
		const auto file = download("http://zlib.net/zlib-" + versions::zlib() + ".tar.gz");
		decompress(file, src_path());
	}

	void do_build()
	{
		cmake_for_nmake().run(src_path(), "", src_path());
		nmake(src_path() / cmake_for_nmake::build_path());
	}

	void do_install()
	{
		nmake_install(src_path() / cmake_for_nmake::build_path());
		op::copy_file_to_dir(src_path() / cmake_for_nmake::build_path() / "zconf.h", src_path());
	}

private:
	fs::path src_path()
	{
		return paths::build() / ("zlib-" + versions::zlib());
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
	void do_fetch()
	{
		const auto underscores = replace_all(versions::boost(), ".", "_");

		const auto file = download(
			"https://github.com/ModOrganizer2/modorganizer-umbrella/"
			"releases/download/1.1/boost_prebuilt_" + underscores + ".7z");

		const auto dir = paths::build() / ("boost_" + underscores);
		decompress(file, dir);
	}

	void do_build()
	{
		op::copy_file_to_dir(lib_path() / python_dll(), paths::install_bin());
	}

private:
	fs::path src_path() const
	{
		const auto underscores = replace_all(versions::boost(), ".", "_");
		return paths::build() / ("boost_" + underscores);
	}

	fs::path lib_path() const
	{
		const std::string lib = "lib64-msvc-" + versions::boost_vs();
		return src_path() / lib / "lib";
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
	void do_fetch()
	{
		const auto file = download(
			"https://github.com/fmtlib/fmt/releases/download/" +
			versions::fmt() + "/fmt-" + versions::fmt() + ".zip");

		decompress(file, src_path());
	}

	void do_build()
	{
		cmake_for_nmake().run(src_path(), "-DFMT_TEST=OFF -DFMT_DOC=OFF");
		nmake(src_path() / cmake_for_nmake::build_path());
	}

private:
	fs::path src_path()
	{
		return paths::build() / ("fmt-" + versions::fmt());
	}
};


int run()
{
	try
	{
		vcvars();

		//sevenz().run();
		//zlib().run();
		//boost().run();
		fmt().run();

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
	//std::cin.get();
	return r;
}

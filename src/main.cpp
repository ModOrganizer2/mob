#include "pch.h"
#include "conf.h"
#include "net.h"
#include "op.h"
#include "tools.h"
#include "utility.h"

namespace builder
{

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
			(conf::verbose() ? "" : "/C /S ") +
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

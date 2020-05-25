#include "pch.h"
#include "tasks.h"

namespace mob
{

fmt::fmt()
	: basic_task("fmt")
{
}

std::string fmt::version()
{
	return conf::version_by_name("fmt");
}

bool fmt::prebuilt()
{
	return false;
}

fs::path fmt::source_path()
{
	return paths::build() / ("fmt-" + version());
}

fs::path fmt::solution_path()
{
	const auto build_path = create_cmake_tool(source_path()).build_path();
	return build_path / "INSTALL.vcxproj";
}

void fmt::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (is_set(c, clean::redownload))
			run_tool(downloader(source_url(), downloader::clean));

		if (is_set(c, clean::reconfigure))
			run_tool(create_cmake_tool(source_path(), cmake::clean));

		if (is_set(c, clean::rebuild))
			run_tool(create_msbuild_tool(msbuild::clean));
	});
}

void fmt::do_fetch()
{
	const auto file = instrument<times::fetch>([&]
	{
		return run_tool(downloader(source_url()));
	});

	instrument<times::extract>([&]
	{
		run_tool(extractor()
			.file(file)
			.output(source_path()));
	});
}

cmake fmt::create_cmake_tool(const fs::path& src_path, cmake::ops o)
{
	return std::move(cmake(o)
		.generator(cmake::vs)
		.root(src_path)
		.prefix(src_path / "build")
		.def("FMT_TEST", "OFF")
		.def("FMT_DOC", "OFF"));
}

msbuild fmt::create_msbuild_tool(msbuild::ops o)
{
	return std::move(msbuild(o)
		.solution(solution_path()));
}

void fmt::do_build_and_install()
{
	const auto build_path = instrument<times::configure>([&]
	{
		return run_tool(create_cmake_tool(source_path()));
	});

	instrument<times::build>([&]
	{
		run_tool(create_msbuild_tool());
	});
}

url fmt::source_url()
{
	return
		"https://github.com/fmtlib/fmt/releases/download/" +
		version() + "/fmt-" + version() + ".zip";
}

}	// namespace

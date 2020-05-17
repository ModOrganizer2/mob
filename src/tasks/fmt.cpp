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
	return version_by_name("fmt");
}

bool fmt::prebuilt()
{
	return false;
}

fs::path fmt::source_path()
{
	return paths::build() / ("fmt-" + version());
}

void fmt::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void fmt::do_clean_for_rebuild()
{
	cmake::clean(cx(), source_path());
}

void fmt::do_build_and_install()
{
	const auto build_path = run_tool(cmake()
		.generator(cmake::vs)
		.root(source_path())
		.prefix(source_path() / "build")
		.def("FMT_TEST", "OFF")
		.def("FMT_DOC", "OFF"));

	run_tool(msbuild()
		.solution(build_path / "INSTALL.vcxproj"));
}

url fmt::source_url()
{
	return
		"https://github.com/fmtlib/fmt/releases/download/" +
		version() + "/fmt-" + version() + ".zip";
}

}	// namespace

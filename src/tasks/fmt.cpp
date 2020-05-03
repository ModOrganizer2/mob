#include "pch.h"
#include "tasks.h"

namespace mob
{

fmt::fmt()
	: basic_task("fmt")
{
}

fs::path fmt::source_path()
{
	return paths::build() / ("fmt-" + versions::fmt());
}

void fmt::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void fmt::do_build_and_install()
{
	const auto build_path = run_tool(cmake()
		.generator(cmake::jom)
		.root(source_path())
		.def("FMT_TEST=OFF")
		.def("FMT_DOC=OFF"));

	run_tool(jom().path(build_path));
}

url fmt::source_url()
{
	return
		"https://github.com/fmtlib/fmt/releases/download/" +
		versions::fmt() + "/fmt-" + versions::fmt() + ".zip";
}

}	// namespace

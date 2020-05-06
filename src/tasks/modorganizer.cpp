#include "pch.h"
#include "tasks.h"

namespace mob
{

static std::mutex g_super_mutex;
static std::atomic<bool> g_super_initialized = false;

modorganizer::modorganizer(std::string long_name)
	: basic_task(long_name)
{
	auto s = short_name();
	if (s != name())
		add_name(s);
}

fs::path modorganizer::source_path()
{
	return {};
}

fs::path modorganizer::this_source_path() const
{
	return super_path() / short_name();
}

fs::path modorganizer::super_path()
{
	return paths::build() / "modorganizer_super";
}

void modorganizer::do_fetch()
{
	initialize_super(super_path());

	if (fs::exists(this_source_path() / ".git"))
	{
		run_tool(git_clone()
			.url(make_github_url(conf::mo_org(), name()))
			.branch(conf::mo_branch())
			.output(this_source_path()));
	}
	else
	{
		run_tool(process_runner(process()
			.binary(tools::git())
			.arg("-c", "core.autocrlf=false")
			.arg("submodule")
			.arg("--quiet")
			.arg("add")
			.arg("-b", conf::mo_branch())
			.arg("--force")
			.arg("--name", short_name())
			.arg(make_github_url(conf::mo_org(), name()))
			.arg(short_name())
			.cwd(super_path())));
	}
}

void modorganizer::do_build_and_install()
{
	if (!fs::exists(this_source_path() / "CMakeLists.txt"))
	{
		cx().trace(context::generic,
			name() + " has no CMakeLists.txt, not running cmake");

		return;
	}

	const auto build_path = run_tool(cmake()
		.generator(cmake::vs)
		.def("CMAKE_INSTALL_PREFIX:PATH", paths::install())
		.def("DEPENDENCIES_DIR",   paths::build())
		.def("BOOST_ROOT",         boost::source_path())
		.def("Boost_LIBRARY_DIRS", boost::lib_path(arch::x64))
		.def("BOOST_LIBRARYDIR",   boost::lib_path(arch::x64))
		.def("FMT_ROOT",           fmt::source_path())
		.def("SPDLOG_ROOT",        spdlog::source_path())
		.def("LOOT_PATH",          libloot::source_path())
		.def("LZ4_ROOT",           lz4::source_path())
		.def("QT_ROOT",            paths::qt_install())
		.def("ZLIB_ROOT",          zlib::source_path())
		.def("PYTHON_ROOT",        python::source_path())
		.def("SEVENZ_ROOT",        sevenz::source_path())
		.def("LIBBSARCH_ROOT",     libbsarch::source_path())
		.root(this_source_path()));

	// run the project file instead of the .sln and giving INSTALL as a
	// target, because the target name depends on the folders in the solution
	//
	// since cmake can put INSTALL inside CMakePredefinedTarget, the target has
	// to be "CMakePredefinedTarget\\INSTALL" instead of just "INSTALL"
	//
	// because the creation of the CMakePredefinedTarget actually depends on
	// the USE_FOLDERS variable in the cmake file, just use the project
	// instead
	run_tool(msbuild()
		.solution(build_path / ("INSTALL.vcxproj"))
		.config("RelWithDebInfo")
		.architecture(arch::x64));
}

void modorganizer::initialize_super(const fs::path& super_root)
{
	std::scoped_lock lock(g_super_mutex);
	if (g_super_initialized)
		return;

	g_super_initialized = true;

	cx().trace(context::generic, "checking super");

	auto p = process()
		.binary(tools::git())
		.arg("rev-parse")
		.arg("--is-inside-work-tree")
		.stderr_filter([](process::filter& f)
		{
			if (f.line.find("not a git repo") != std::string::npos)
				f.lv = context::level::trace;
		})
		.flags(process::allow_failure)
		.cwd(super_root);

	if (run_tool(process_runner(p)) == 0)
	{
		cx().debug(context::generic, "super already initialized");
		return;
	}

	cx().trace(context::generic, "initializing super");

	run_tool(process_runner(process()
		.binary(tools::git())
		.arg("init")
		.cwd(super_root)));
}

std::string modorganizer::short_name() const
{
	const auto dash = name().find("-");
	if (dash == std::string::npos)
		return name();

	return name().substr(dash + 1);
}

}	// namespace
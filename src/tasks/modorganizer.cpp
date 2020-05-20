#include "pch.h"
#include "tasks.h"

namespace mob
{

static std::mutex g_super_mutex;
static std::atomic<bool> g_super_initialized = false;

std::string make_short_name(const std::string& name)
{
	const auto dash = name.find("-");
	if (dash == std::string::npos)
		return name;

	return name.substr(dash + 1);
}


modorganizer::modorganizer(std::string long_name)
	: basic_task(make_short_name(long_name)), repo_(long_name)
{
	if (long_name != name())
		add_name(long_name);
}

std::string modorganizer::version()
{
	return {};
}

bool modorganizer::prebuilt()
{
	return false;
}

bool modorganizer::is_super() const
{
	return true;
}

fs::path modorganizer::source_path()
{
	return {};
}

fs::path modorganizer::this_source_path() const
{
	return super_path() / name();
}

fs::path modorganizer::super_path()
{
	return paths::build() / "modorganizer_super";
}

void modorganizer::do_fetch()
{
	instrument(times_.init_super, [&]
	{
		initialize_super(super_path());
	});

	instrument(times_.fetch, [&]
	{
		run_tool(task_conf().make_git()
			.url(make_github_url(task_conf().mo_org(), repo_))
			.branch(task_conf().mo_branch())
			.root(this_source_path()));
	});
}

cmake modorganizer::create_cmake_tool(const fs::path& root)
{
	cmake m;

	m
		.generator(cmake::vs)
		.def("CMAKE_INSTALL_PREFIX:PATH", paths::install())
		.def("DEPENDENCIES_DIR",   paths::build())
		.def("BOOST_ROOT",         boost::source_path())
		.def("BOOST_LIBRARYDIR",   boost::lib_path(arch::x64))
		.def("FMT_ROOT",           fmt::source_path())
		.def("SPDLOG_ROOT",        spdlog::source_path())
		.def("LOOT_PATH",          libloot::source_path())
		.def("LZ4_ROOT",           lz4::source_path())
		.def("QT_ROOT",            qt::installation_path())
		.def("ZLIB_ROOT",          zlib::source_path())
		.def("PYTHON_ROOT",        python::source_path())
		.def("SEVENZ_ROOT",        sevenz::source_path())
		.def("LIBBSARCH_ROOT",     libbsarch::source_path())
		.def("BOOST_DI_ROOT",      boost_di::source_path())
		.def("GTEST_ROOT",         gtest::source_path())
		.root(root);

	return m;
}

void modorganizer::do_build_and_install()
{
	{
		std::unique_lock<std::mutex> lock(g_super_mutex, std::defer_lock);

		instrument(times_.add_submodule_lock, [&]
		{
			lock.lock();
		});

		instrument(times_.add_submodule, [&]
		{
			run_tool(task_conf().make_git(git::ops::add_submodule)
				.url(make_github_url(task_conf().mo_org(), repo_))
				.branch(task_conf().mo_branch())
				.submodule_name(name())
				.root(super_path()));
		});
	}

	if (!fs::exists(this_source_path() / "CMakeLists.txt"))
	{
		cx().trace(context::generic,
			"{} has no CMakeLists.txt, not building", repo_);

		return;
	}

	const auto build_path = instrument(times_.configure, [&]
	{
		return run_tool(cmake()
			.generator(cmake::vs)
			.def("CMAKE_INSTALL_PREFIX:PATH", paths::install())
			.def("DEPENDENCIES_DIR",   paths::build())
			.def("BOOST_ROOT",         boost::source_path())
			.def("BOOST_LIBRARYDIR",   boost::lib_path(arch::x64))
			.def("FMT_ROOT",           fmt::source_path())
			.def("SPDLOG_ROOT",        spdlog::source_path())
			.def("LOOT_PATH",          libloot::source_path())
			.def("LZ4_ROOT",           lz4::source_path())
			.def("QT_ROOT",            qt::installation_path())
			.def("ZLIB_ROOT",          zlib::source_path())
			.def("PYTHON_ROOT",        python::source_path())
			.def("SEVENZ_ROOT",        sevenz::source_path())
			.def("LIBBSARCH_ROOT",     libbsarch::source_path())
			.def("BOOST_DI_ROOT",      boost_di::source_path())
			.def("GTEST_ROOT",         gtest::source_path())
			.root(this_source_path()));
	});

	// run the project file instead of the .sln and giving INSTALL as a
	// target, because the target name depends on the folders in the solution
	//
	// since cmake can put INSTALL inside CMakePredefinedTarget, the target has
	// to be "CMakePredefinedTarget\\INSTALL" instead of just "INSTALL"
	//
	// because the creation of the CMakePredefinedTarget actually depends on
	// the USE_FOLDERS variable in the cmake file, just use the project
	// instead
	instrument(times_.build, [&]
	{
		run_tool(msbuild()
			.solution(build_path / ("INSTALL.vcxproj"))
			.config("RelWithDebInfo")
			.architecture(arch::x64));
	});
}

void modorganizer::initialize_super(const fs::path& super_root)
{
	std::scoped_lock lock(g_super_mutex);
	if (g_super_initialized)
		return;

	g_super_initialized = true;

	cx().trace(context::generic, "checking super");

	if (git::is_git_repo(super_root))
	{
		cx().debug(context::generic, "super already initialized");
		return;
	}

	cx().trace(context::generic, "initializing super");
	git::init_repo(super_root);
}

}	// namespace

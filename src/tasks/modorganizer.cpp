#include "pch.h"
#include "tasks.h"

namespace mob::tasks
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


modorganizer::modorganizer(std::string long_name, flags f)
	: modorganizer(std::vector<std::string>{long_name}, f)
{
}

modorganizer::modorganizer(std::vector<const char*> names, flags f)
	: modorganizer(std::vector<std::string>(names.begin(), names.end()), f)
{
}

modorganizer::modorganizer(std::vector<std::string> names, flags f)
	: basic_task(make_short_name(names[0])), repo_(names[0]), flags_(f)
{
	for (auto&& n : names)
		add_name(std::move(n));
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

bool modorganizer::is_gamebryo_plugin() const
{
	return is_set(flags_, gamebryo);
}

bool modorganizer::is_nuget_plugin() const
{
	return is_set(flags_, nuget);
}

fs::path modorganizer::source_path()
{
	return {};
}

fs::path modorganizer::this_source_path() const
{
	return super_path() / name();
}

fs::path modorganizer::this_solution_path() const
{
	const auto build_path = create_cmake_tool(this_source_path()).build_path();
	return build_path / "INSTALL.vcxproj";
}

fs::path modorganizer::super_path()
{
	return conf().path().build() / "modorganizer_super";
}

url modorganizer::git_url() const
{
	return task_conf().make_git_url(task_conf().mo_org(), repo_);
}

std::string modorganizer::org() const
{
	return task_conf().mo_org();
}

std::string modorganizer::repo() const
{
	return repo_;
}

void modorganizer::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (is_set(c, clean::reclone))
		{
			git_wrap::delete_directory(cx(), this_source_path());
			return;
		}

		if (is_set(c, clean::reconfigure))
			run_tool(create_this_cmake_tool(cmake::clean));

		if (is_set(c, clean::rebuild))
			run_tool(create_this_msbuild_tool(msbuild::clean));
	});
}

void modorganizer::do_fetch()
{
	instrument<times::init_super>([&]
	{
		initialize_super(super_path());
	});

	instrument<times::fetch>([&]
	{
		run_tool(task_conf().make_git()
			.url(git_url())
			.branch(task_conf().mo_branch())
			.root(this_source_path()));
	});
}

void modorganizer::do_build_and_install()
{
	git_submodule_adder::instance().queue(std::move(
		git_submodule()
			.url(git_url())
			.branch(task_conf().mo_branch())
			.submodule(name())
			.root(super_path())));

	if (!fs::exists(this_source_path() / "CMakeLists.txt"))
	{
		cx().trace(context::generic,
			"{} has no CMakeLists.txt, not building", repo_);

		return;
	}

	instrument<times::configure>([&]
	{
		run_tool(create_this_cmake_tool());
	});

	// until https://gitlab.kitware.com/cmake/cmake/-/issues/20646 is resolved,
	// we need a manual way of running the msbuild -t:restore
	if (is_nuget_plugin()) {
		instrument<times::build>([&]
		{
			run_tool(create_this_msbuild_tool().targets({ "restore" }));
		});
	}

	instrument<times::build>([&]
	{
		run_tool(create_this_msbuild_tool());
	});
}

cmake modorganizer::create_this_cmake_tool(cmake::ops o)
{
	return create_cmake_tool(this_source_path(), o);
}

cmake modorganizer::create_cmake_tool(const fs::path& root, cmake::ops o)
{
	return std::move(cmake(o)
		.generator(cmake::vs)
		.def("CMAKE_INSTALL_PREFIX:PATH", conf().path().install())
		.def("DEPENDENCIES_DIR",          conf().path().build())
		.def("BOOST_ROOT",                boost::source_path())
		.def("BOOST_LIBRARYDIR",          boost::lib_path(arch::x64))
		.def("FMT_ROOT",                  fmt::source_path())
		.def("SPDLOG_ROOT",               spdlog::source_path())
		.def("LOOT_PATH",                 libloot::source_path())
		.def("LZ4_ROOT",                  lz4::source_path())
		.def("QT_ROOT",                   qt::installation_path())
		.def("ZLIB_ROOT",                 zlib::source_path())
		.def("PYTHON_ROOT",               python::source_path())
		.def("SEVENZ_ROOT",               sevenz::source_path())
		.def("LIBBSARCH_ROOT",            libbsarch::source_path())
		.def("BOOST_DI_ROOT",             boost_di::source_path())
		.def("GTEST_ROOT",                gtest::source_path())
		.root(root));
}

msbuild modorganizer::create_this_msbuild_tool(msbuild::ops o)
{
	return std::move(msbuild(o)
		.solution(this_solution_path())
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

	git_wrap g(super_root);

	if (g.is_git_repo())
	{
		cx().debug(context::generic, "super already initialized");
		return;
	}

	cx().trace(context::generic, "initializing super");
	g.init_repo();
}

}	// namespace

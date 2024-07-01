#include "pch.h"
#include "tasks.h"

namespace mob::tasks {

    // given a vector of names (some projects have more than one, see add_tasks() in
    // main.cpp), this prepends the simplified name to the vector and returns it
    //
    // most MO project names are something like "modorganizer-uibase" on github and
    // the simplified name is used for two main reasons:
    //
    //  1) individual directories in modorganizer_super have historically used the
    //     simplified name only
    //
    //  2) it's useful to have an simplified name for use on the command line
    //
    std::vector<std::string> make_names(std::vector<std::string> names)
    {
        // first name in the list might be a "modorganizer-something"
        const auto main_name = names[0];

        const auto dash = main_name.find("-");
        if (dash != std::string::npos) {
            // remove the part before the dash and the dash
            names.insert(names.begin(), main_name.substr(dash + 1));
        }

        return names;
    }

    // creates the repo in modorganizer_super, used to add submodules
    //
    // only one task will end up past the mutex and the flag, so it's only done
    // once
    //
    void initialize_super(context& cx, const fs::path& super_root)
    {
        static std::mutex mutex;
        static bool initialized = false;

        std::scoped_lock lock(mutex);
        if (initialized)
            return;

        initialized = true;

        cx.trace(context::generic, "checking super");

        git_wrap g(super_root);

        // happens when running mob again in the same build tree
        if (g.is_git_repo()) {
            cx.debug(context::generic, "super already initialized");
            return;
        }

        // create empty repo
        cx.trace(context::generic, "initializing super");
        g.init_repo();
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
        : task(make_names(names)), repo_(names[0]), flags_(f)
    {
        if (names.size() > 1) {
            project_ = names[1];
        }
        else {
            project_ = make_names(names)[0];
        }
    }

    bool modorganizer::is_gamebryo_plugin() const
    {
        return is_set(flags_, gamebryo);
    }

    bool modorganizer::is_nuget_plugin() const
    {
        return is_set(flags_, nuget);
    }

    fs::path modorganizer::source_path() const
    {
        // something like build/modorganizer_super/uibase
        return super_path() / name();
    }

    fs::path modorganizer::project_file_path() const
    {
        // ask cmake for the build path it would use
        const auto build_path = create_cmake_tool(source_path()).build_path();

        // use the INSTALL project
        return build_path / (project_ + ".sln");
    }

    fs::path modorganizer::super_path()
    {
        return conf().path().build() / "modorganizer_super";
    }

    url modorganizer::git_url() const
    {
        return make_git_url(task_conf().mo_org(), repo_);
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
        // delete the whole directory
        if (is_set(c, clean::reclone)) {
            git_wrap::delete_directory(cx(), source_path());

            // no need to do anything else
            return;
        }

        // cmake clean
        if (is_set(c, clean::reconfigure))
            run_tool(create_cmake_tool(cmake::clean));

        // msbuild clean
        if (is_set(c, clean::rebuild))
            run_tool(create_msbuild_tool(msbuild::clean));
    }

    void modorganizer::do_fetch()
    {
        // make sure the super directory is initialized, only done once
        initialize_super(cx(), super_path());

        // find the best suitable branch
        const auto fallback = task_conf().mo_fallback_branch();
        auto branch         = task_conf().mo_branch();
        if (!fallback.empty() && !git_wrap::remote_branch_exists(git_url(), branch)) {
            cx().warning(context::generic,
                         "{} has no remote {} branch, switching to {}", repo_, branch,
                         fallback);
            branch = fallback;
        }

        // clone/pull
        run_tool(make_git().url(git_url()).branch(branch).root(source_path()));
    }

    void modorganizer::do_build_and_install()
    {
        // adds a git submodule in modorganizer_super for this project; note that
        // git_submodule_adder runs a thread because adding submodules is slow, but
        // can happen while stuff is building
        git_submodule_adder::instance().queue(
            std::move(git_submodule()
                          .url(git_url())
                          .branch(task_conf().mo_branch())
                          .submodule(name())
                          .root(super_path())));

        // not all modorganizer projects need to actually be built, such as
        // cmake_common, so don't try if there's no cmake file
        if (!fs::exists(source_path() / "CMakeLists.txt")) {
            cx().trace(context::generic, "{} has no CMakeLists.txt, not building",
                       repo_);

            return;
        }

        // run cmake
        run_tool(create_cmake_tool());

        // run restore for nuget
        //
        // until https://gitlab.kitware.com/cmake/cmake/-/issues/20646 is resolved,
        // we need a manual way of running the msbuild -t:restore
        if (is_nuget_plugin())
            run_tool(create_msbuild_tool().targets({"restore"}));

        // run msbuild
        run_tool(create_msbuild_tool());
    }

    cmake modorganizer::create_cmake_tool(cmake::ops o)
    {
        return create_cmake_tool(source_path(), o, task_conf().configuration());
    }

    cmake modorganizer::create_cmake_tool(const fs::path& root, cmake::ops o, config c)
    {
        return std::move(
            cmake(o)
                .generator(cmake::vs)
                .def("CMAKE_INSTALL_PREFIX:PATH", conf().path().install())
                .def("DEPENDENCIES_DIR", conf().path().build())
                .def("BOOST_ROOT", boost::source_path())
                .def("BOOST_LIBRARYDIR", boost::lib_path(arch::x64))
                .def("SPDLOG_ROOT", spdlog::source_path())
                .def("LOOT_PATH", libloot::source_path())
                .def("LZ4_ROOT", lz4::source_path())
                .def("QT_ROOT", qt::installation_path())
                .def("ZLIB_ROOT", zlib::source_path())
                .def("PYTHON_ROOT", python::source_path())
                .def("SEVENZ_ROOT", sevenz::source_path())
                .def("LIBBSARCH_ROOT", libbsarch::source_path())
                .def("BOOST_DI_ROOT", boost_di::source_path())
                // gtest has no RelWithDebInfo, so simply use Debug/Release
                .def("GTEST_ROOT",
                     gtest::build_path(arch::x64, c == config::debug ? config::debug
                                                                     : config::release))
                .def("OPENSSL_ROOT_DIR", openssl::source_path())
                .def("DIRECTXTEX_ROOT", directxtex::source_path())
                .root(root));
    }

    msbuild modorganizer::create_msbuild_tool(msbuild::ops o)
    {
        return std::move(msbuild(o)
                             .solution(project_file_path())
                             .configuration(task_conf().configuration())
                             .architecture(arch::x64));
    }

}  // namespace mob::tasks

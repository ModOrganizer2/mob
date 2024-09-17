#include "pch.h"
#include "tasks.h"

namespace mob::tasks {

    // build CMAKE_PREFIX_PATH for MO2 tasks
    //
    std::string cmake_prefix_path()
    {
        return conf().path().qt_install().string() + ";" +
               (modorganizer::super_path() / "cmake_common").string() + ";" +
               (conf().path().install() / "lib" / "cmake").string();
    }

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

    modorganizer::modorganizer(std::string long_name)
        : modorganizer(std::vector<std::string>{long_name})
    {
    }

    modorganizer::modorganizer(std::vector<const char*> names)
        : modorganizer(std::vector<std::string>(names.begin(), names.end()))
    {
    }

    modorganizer::modorganizer(std::vector<std::string> names)
        : task(make_names(names)), repo_(names[0])
    {
        if (names.size() > 1) {
            project_ = names[1];
        }
        else {
            project_ = make_names(names)[0];
        }
    }

    fs::path modorganizer::source_path() const
    {
        // something like build/modorganizer_super/uibase
        return super_path() / name();
    }

    fs::path modorganizer::super_path()
    {
        return conf().path().build();
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
            run_tool(cmake(cmake::clean).root(source_path()));
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
        // adds a git submodule in build for this project; note that
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
        if (!exists(source_path() / "CMakeLists.txt")) {
            cx().trace(context::generic, "{} has no CMakeLists.txt, not building",
                       repo_);

            return;
        }

        // if there is a CMakeLists.txt, there must be a CMakePresets.json otherwise
        // we cannot build
        if (!exists(source_path() / "CMakePresets.json")) {
            gcx().bail_out(context::generic,
                           "{} has no CMakePresets.txt, aborting build", repo_);
        }

        // run cmake
        run_tool(cmake(cmake::generate)
                     .generator(cmake::vs)
                     .def("CMAKE_INSTALL_PREFIX:PATH", conf().path().install())
                     .def("CMAKE_PREFIX_PATH", cmake_prefix_path())
                     .preset("vs2022-windows")
                     .root(source_path()));

        // run cmake --build with default target
        // TODO: handle rebuild by adding `--clean-first`
        // TODO: have a way to specify the `--parallel` value - 16 is useful to build
        // game_bethesda that has 15 games, so 15 projects
        run_tool(cmake(cmake::build)
                     .root(source_path())
                     .arg("--parallel")
                     .arg("16")
                     .configuration(mob::config::relwithdebinfo));

        // run cmake --install
        run_tool(cmake(cmake::install)
                     .root(source_path())
                     .configuration(mob::config::relwithdebinfo));
    }

}  // namespace mob::tasks

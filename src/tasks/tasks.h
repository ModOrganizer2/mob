#pragma once

#include "../core/conf.h"
#include "../core/op.h"
#include "../net.h"
#include "../tools/tools.h"
#include "../utility.h"
#include "task.h"

namespace mob::tasks {

    // single header for all the tasks, not worth having a header per task

    class explorerpp : public basic_task<explorerpp> {
    public:
        explorerpp();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
    };

    class installer : public basic_task<installer> {
    public:
        installer();

        static bool prebuilt();
        static std::string version();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;
    };

    class licenses : public task {
    public:
        licenses();

    protected:
        void do_build_and_install() override;
    };

    // a task for all modorganizer projects except for the installer, which is
    // technically an MO project but is built differently
    //
    // note that this doesn't inherit from basic_task because it doesn't have the
    // usual static functions for source_path() and prebuilt() since there's a
    // variety of modorganizer objects, one per project
    //
    class modorganizer : public task {
    public:
        // build CMAKE_PREFIX_PATH for MO2 tasks
        //
        static std::string cmake_prefix_path();

        // path of the root modorganizer_super directory
        //
        static fs::path super_path();

        // creates the same cmake tool as the one used to build the various MO
        // projects, used internally, but also by the cmake command
        //
        static cmake create_cmake_tool(const fs::path& root,
                                       cmake::ops o  = cmake::generate,
                                       config config = config::relwithdebinfo);

        // some mo tasks have more than one name, mostly because the transifex slugs
        // are different than the names on github; the std::string and const char*
        // overloads are because they're constructed from initializer lists and it's
        // more convenient that way
        modorganizer(std::string name);
        modorganizer(std::vector<std::string> names);
        modorganizer(std::vector<const char*> names);

        // url to the git repo
        //
        url git_url() const;

        // `mo_org` setting from the ini (typically ModOrganizer2)
        //
        std::string org() const;

        // name of the repo on github (first name given in the constructor,
        // something like "cmake_common" or "modorganizer-uibase")
        //
        std::string repo() const;

        // directory for the project's source, something like
        // "build/modorganizer_super/modorganizer"
        //
        fs::path source_path() const;

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        std::string repo_;
        std::string project_;
    };

    class stylesheets : public task {
    public:
        struct release {
            std::string user;
            std::string repo;
            std::string version;
            std::string file;
            std::string top_level_folder;
        };

        stylesheets();

        static bool prebuilt();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        downloader make_downloader_tool(const release& r,
                                        downloader::ops = downloader::download) const;

        fs::path release_build_path(const release& r) const;
    };

    // see translations.cpp for more info
    //
    class translations : public task {
    public:
        // given the root translations folder, will have one `project` object per
        // directory
        //
        // each project will have a list of `lang` objects, each lang has the list
        // of .ts files that need to be compiled to create the .qm file
        //
        class projects {
        public:
            // a language for a project
            struct lang {
                // language name
                std::string name;

                // .ts files that need to be compiled
                std::vector<fs::path> ts_files;

                lang(std::string n);

                // if `name` has an underscore, returns the part before and after
                // it; if there is no underscore, first is `name`, second is empty
                //
                std::pair<std::string, std::string> split() const;
            };

            // a project that contains languages
            struct project {
                // project name
                std::string name;

                // list of languages
                std::vector<lang> langs;

                project(std::string n = {});
            };

            // walks the given directory and figures out projects and languages
            //
            projects(fs::path root);

            // list of projects found, one per directory in the root
            //
            const std::vector<project>& get() const;

            // any warnings that happened while walking the directories
            //
            const std::vector<std::string>& warnings() const;

            // return a project by name
            //
            std::optional<project> find(std::string_view name) const;

        private:
            // translations directory
            const fs::path root_;

            // projects
            std::vector<project> projects_;

            // warnings
            std::vector<std::string> warnings_;

            // set of files for which a warning was added to warnings_, avoids
            // duplicate warnings
            std::set<fs::path> warned_;

            // parses the directory name, walks all the .ts files, returns a project
            // object for them
            //
            project create_project(const fs::path& dir);

            // returns a lang object that contains at least the given main_ts_file,
            // but might contain more if it's a gamebryo plugin
            //
            lang create_lang(const std::string& project_name,
                             const fs::path& main_ts_file);
        };

        translations();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        // copy builtin qt .qm files
        void copy_builtin_qt_translations(const projects::project& organizer_project,
                                          const fs::path& dest);
    };

    class usvfs : public basic_task<usvfs> {
    public:
        usvfs();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        void fetch_from_source();
        void build_and_install_from_source();

        cmake create_cmake_tool(arch, cmake::ops = cmake::generate) const;
        msbuild create_msbuild_tool(arch, msbuild::ops = msbuild::build,
                                    config = config::release) const;
    };

}  // namespace mob::tasks

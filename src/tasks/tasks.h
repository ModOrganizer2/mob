#pragma once

#include "../core/conf.h"
#include "../core/op.h"
#include "../net.h"
#include "../tools/tools.h"
#include "../utility.h"
#include "task.h"

namespace mob::tasks {

    // single header for all the tasks, not worth having a header per task

    class boost : public basic_task<boost> {
    public:
        struct version_info {
            std::string major, minor, patch, rest;
        };

        boost();

        static version_info parsed_version();
        static std::string version();
        static std::string version_vs();
        static bool prebuilt();

        static fs::path source_path();
        static fs::path lib_path(arch a);
        static fs::path root_lib_path(arch a);

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        void fetch_prebuilt();
        void build_and_install_prebuilt();

        void fetch_from_source();
        void build_and_install_from_source();
        void write_config_jam();

        void bootstrap();

        void do_b2(const std::vector<std::string>& components, const std::string& link,
                   const std::string& runtime_link, arch a);
    };

    // needed by bsapacker
    //
    class boost_di : public basic_task<boost_di> {
    public:
        boost_di();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
    };

    // needed by python
    //
    class bzip2 : public basic_task<bzip2> {
    public:
        bzip2();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
    };

    class directxtex : public basic_task<directxtex> {
    public:
        directxtex();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;
    };

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

    class gtest : public basic_task<gtest> {
    public:
        gtest();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();
        static fs::path build_path(arch = arch::x64, config = config::release);

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;
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

    class libbsarch : public basic_task<libbsarch> {
    public:
        libbsarch();

        static std::string version();
        static config build_type();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;
    };

    class libloot : public basic_task<libloot> {
    public:
        libloot();

        static std::string version();
        static std::string hash();
        static std::string branch();

        static bool prebuilt();
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

    class lz4 : public basic_task<lz4> {
    public:
        lz4();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        void fetch_prebuilt();
        void build_and_install_prebuilt();

        void fetch_from_source();
        void build_and_install_from_source();

        msbuild create_msbuild_tool(msbuild::ops o = msbuild::build);
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

    class ncc : public basic_task<ncc> {
    public:
        ncc();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        msbuild create_msbuild_tool(msbuild::ops o);
    };

    class nmm : public basic_task<nmm> {
    public:
        nmm();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        msbuild create_msbuild_tool(msbuild::ops o     = msbuild::build,
                                    msbuild::flags_t f = msbuild::noflags);
    };

    class openssl : public basic_task<openssl> {
    public:
        struct version_info {
            std::string major, minor, patch;
        };

        openssl();

        static version_info parsed_version();
        static std::string version();
        static bool prebuilt();

        static fs::path source_path();
        static fs::path include_path();
        static fs::path build_path();
        static fs::path bin_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        void fetch_prebuilt();
        void fetch_from_source();
        void build_and_install_prebuilt();
        void build_and_install_from_source();

        void configure();
        void install_engines();
        void copy_files();
        void copy_dlls_to(const fs::path& dir);
        void copy_pdbs_to(const fs::path& dir);
    };

    // when building from source, builds both pyqt5 and pyqt5-sip; when using the
    // prebuilt, the downloaded zip contains both
    //
    class pyqt : public basic_task<pyqt> {
    public:
        pyqt();

        static std::string version();
        static std::string builder_version();
        static bool prebuilt();

        static fs::path source_path();
        static fs::path build_path();
        static config build_type();

        // "PyQt5.sip", used both in pyqt and sip
        //
        static std::string pyqt_sip_module_name();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        void fetch_prebuilt();
        void fetch_from_source();
        void build_and_install_prebuilt();
        void build_and_install_from_source();

        void sip_build();
        void install_sip_file();
        void copy_files();
    };

    class python : public basic_task<python> {
    public:
        struct version_info {
            std::string major;
            std::string minor;
            std::string patch;
        };

        python();

        static std::string version();
        static bool prebuilt();
        static version_info parsed_version();

        // build/python-XX
        //
        static fs::path source_path();

        // build/python-XX/PCBuild/amd64
        //
        static fs::path build_path();

        // build/python-XX/PCBuild/amd64/python.exe
        //
        static fs::path python_exe();

        // build/python-XX/Include
        //
        static fs::path include_path();

        // build/python-XX/Scripts
        //
        static fs::path scripts_path();

        // build/python-XX/Lib/site-packages
        //
        static fs::path site_packages_path();

        // configuration to build
        //
        static config build_type();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        void fetch_prebuilt();
        void fetch_from_source();
        void build_and_install_prebuilt();
        void build_and_install_from_source();

        void prepare_dependencies();
        void package();
        void install_pip();
        void copy_files();

        msbuild create_msbuild_tool(msbuild::ops o = msbuild::build);
    };

    class pybind11 : public basic_task<pybind11> {
    public:
        pybind11();

        static bool prebuilt();
        static std::string version();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;
    };

    class sevenz : public basic_task<sevenz> {
    public:
        sevenz();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        void build();
    };

    class sip : public basic_task<sip> {
    public:
        sip();

        static std::string version();
        static std::string version_for_pyqt();
        static bool prebuilt();

        static fs::path source_path();
        static fs::path sip_module_exe();
        static fs::path sip_install_exe();
        static fs::path module_source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        void build();
        void generate_header();
        void convert_script_file_to_acp(const std::string& filename);
    };

    class spdlog : public basic_task<spdlog> {
    public:
        spdlog();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
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

    class zlib : public basic_task<zlib> {
    public:
        zlib();

        static std::string version();
        static bool prebuilt();
        static fs::path source_path();

    protected:
        void do_clean(clean c) override;
        void do_fetch() override;
        void do_build_and_install() override;

    private:
        cmake create_cmake_tool(cmake::ops o = cmake::generate);
        msbuild create_msbuild_tool(msbuild::ops o = msbuild::build);
    };

}  // namespace mob::tasks

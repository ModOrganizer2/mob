#pragma once

namespace mob {

    // a tool that runs `cmake ..` by default in a given directory
    //
    // supports either visual studio or jom/nmake and x86/x64 architectures
    //
    class cmake : public basic_process_runner {
    public:
        // path to cmake
        //
        static fs::path binary();

        // type of build files generated
        //
        enum class generators {
            // generates build files for visual studio
            vs = 0x01,

            // generates build files for jom/nmake
            jom = 0x02
        };
        using enum generators;

        // what run() will do
        //
        enum class ops {
            // generates the build files
            generate = 1,

            // build
            build,

            // install
            install,

            // cleans the build files so they're regenerated from scratch
            clean
        };
        using enum ops;

        cmake(ops o = generate);

        // sets the generator, defaults to jom
        //
        cmake& generator(generators g);

        // sets the generator string passed to -G, output() must be set before
        // run() because the output path is only created automatically for known
        // generators from the enum
        //
        cmake& generator(const std::string& g);

        // directory where CMakeLists.txt is
        //
        // by default, the tool will create a build directory in the root with a
        // name based on the generator and architecture (see output()), then cd into
        // it and invoke `cmake ..`
        //
        cmake& root(const fs::path& p);

        // set the targets for build
        //
        cmake& targets(const std::string& target);
        cmake& targets(const std::vector<std::string>& target);

        // set the configuration to build or install
        //
        cmake& configuration(mob::config config);

        // overrides the directory in which cmake will write build files
        //
        // by default, this is a directory inside what was given in root() with a
        // name based on the generator and architecture (such as "vsbuild" or
        // "vsbuild_32")
        //
        // if generator() was called with a string, output() must be called
        //
        cmake& output(const fs::path& p);

        // if not empty, the path is passed to cmake with
        // `-DCMAKE_INSTALL_PREFIX=path`
        //
        cmake& prefix(const fs::path& s);

        // adds a variable definition, passed as `-Dname=value`
        //
        cmake& def(const std::string& name, const std::string& value);
        cmake& def(const std::string& name, const fs::path& p);
        cmake& def(const std::string& name, const char* s);

        // set a preset to run with cmake --preset
        //
        cmake& preset(const std::string& s);

        // adds an arbitrary argument, passed verbatim
        //
        cmake& arg(std::string s);

        // sets the architecture, used along with the generator to create the
        // output directory name, but also to get the proper vcvars environment
        // variables for the build environment
        //
        cmake& architecture(arch a);

        // by default, the tool invokes `cmake ..` in the output directory, setting
        // this will invoke `cmake cmd` instead
        //
        cmake& cmd(const std::string& s);

        // returns the path given in output(), if it was set
        //
        // if not, returns the build path based on the parameters (for example,
        // `vsbuild_32/` for a 32-bit arch with the vs generator
        //
        fs::path build_path() const;

        // returns build_path(), used by task::run_tool()
        //
        fs::path result() const;

    protected:
        // calls either do_clean() or do_generate()
        //
        void do_run() override;

    private:
        // information about a generator available in the `generators` enum,
        // returned by all_generators() below
        //
        // used to generate build directory names and command line parameters
        //
        struct gen_info {
            // name of build directory, "_32" is appended to it for x86
            // architectures
            std::string dir;

            // generator name, passed to -G
            std::string name;

            // names for 32- and 64-bit architectures, passed to -A
            std::string x86;
            std::string x64;

            // if the corresponding string in `x86` and `x64` just above is not
            // empty, returns "-A xxx" depending on the given architecture; returns
            // empty otherwise
            //
            std::string get_arch(arch a) const;

            // for generator that supports it, returns a toolset configuration to set
            // the host as specified in the configuration
            //
            // for VS generator, this returns "-T host=XXX" if conf_host is not empty,
            // otherwise returns an empty string
            //
            std::string get_host(std::string_view conf_host) const;

            // return either `dir` for 64-bit or `dir` + "_32" for 32-bit
            //
            std::string output_dir(arch a) const;
        };

        // what run() does
        ops op_;

        // preset to run
        std::string preset_;

        // directory where CMakeLists.txt is
        fs::path root_;

        // generator used, either from the enum or as a string
        generators gen_;
        std::string genstring_;

        // passed as -DCMAKE_INSTALL_PREFIX
        fs::path prefix_;

        // targets
        std::vector<std::string> targets_;

        // configuration
        mob::config config_{mob::config::relwithdebinfo};

        // passed verbatim
        std::vector<std::string> args_;

        // overrides build directory name
        fs::path output_;

        // architecture, used for build directory name and command line
        arch arch_;

        // overrides `..` on the command line
        std::string cmd_;

        // deletes the build directory
        //
        void do_clean();

        // runs cmake
        //
        void do_generate();
        void do_build();
        void do_install();

        // returns a list of generators handled by this tool, same ones as in the
        // `generators` enum on top
        //
        static const std::map<generators, gen_info>& all_generators();

        // returns the generator info for the given enum value
        //
        static const gen_info& get_generator(generators g);
    };

}  // namespace mob

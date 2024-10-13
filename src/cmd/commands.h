#pragma once

#include "../utility/enum.h"

namespace mob::tasks {
    class modorganizer;
}

namespace mob {

    class task;
    class url;

    // base class for all commands
    //
    class command {
    public:
        // values of options available for all commands
        //
        struct common_options {
            bool dry             = false;
            int output_log_level = -1;
            int file_log_level   = -1;
            std::string log_file;
            std::vector<std::string> options;
            std::vector<std::string> inis;
            bool no_default_inis = false;
            bool dump_inis       = false;
            std::string prefix;
        };

        // returned by meta() by each command
        //
        struct meta_t {
            std::string name, description;
        };

        static common_options common;
        static clipp::group common_options_group();

        virtual ~command() = default;

        // overrides the exit code returned by do_run()
        //
        void force_exit_code(int code);

        // forces this command to show help in run() as if --help had been given
        //
        void force_help();

        // whether this command was entered by the user
        //
        bool picked() const;

        // command line options for this command
        //
        clipp::group group();

        // executes this command
        //
        int run();

        // returns meta information about this command
        //
        virtual meta_t meta() const = 0;

    protected:
        // passed by derived classes in the constructor
        //
        enum flags {
            noflags = 0x00,

            // this command needs the ini loaded before running
            requires_options = 0x01,

            // this command does not handle sigint by itself, run() will hook it
            handle_sigint = 0x02
        };

        MOB_ENUM_FRIEND_OPERATORS(flags);

        // set to true when this command is entered by the user
        bool picked_;

        // set to true with --help or by force_help()
        bool help_;

        command(flags f = noflags);

        // some options have a unique version on the command line because they're
        // used often, such as --dry or -l; those are converted to their ini
        // equivalent here (`-l 3` becomes `global/output_log_level=3` in
        // common.options)
        //
        virtual void convert_cl_to_conf();

        // disables all tasks by setting task/enabled=false, but sets
        // name:task/enabled=true for the given tasks
        //
        void set_task_enabled_flags(const std::vector<std::string>& tasks);

        // calls prepare_options() and loads inis
        //
        int load_options();

        // calls convert_cl_to_conf() and populates inis_
        //
        int prepare_options(bool verbose);

        // called by group(), returns the clipp group for this command
        //
        virtual clipp::group do_group() = 0;

        // called by run(), executes this command
        //
        virtual int do_run() = 0;

        // called by run() when --help is given; should return additional text that
        // is output at the end of the usage info
        //
        virtual std::string do_doc() { return {}; }

    private:
        // flags requested by this command
        flags flags_;

        // exit code set by force_exit_code()
        std::optional<int> code_;

        // list of inis found by gather_inis()
        std::vector<fs::path> inis_;

        // finds all the inis
        //
        int gather_inis(bool verbose);
    };

    // displays mob's version
    //
    class version_command : public command {
    public:
        meta_t meta() const override;

    protected:
        clipp::group do_group() override;
        int do_run() override;
    };

    // displays the usage, list of commands and some additional text
    //
    class help_command : public command {
    public:
        meta_t meta() const override;
        void set_commands(const std::vector<std::shared_ptr<command>>& v);

    protected:
        clipp::group do_group() override;
        int do_run() override;

    private:
        std::string commands_;
    };

    // lists all options and their values
    //
    class options_command : public command {
    public:
        options_command();
        meta_t meta() const override;

    protected:
        clipp::group do_group() override;
        int do_run() override;
        std::string do_doc() override;
    };

    // builds stuff
    //
    class build_command : public command {
    public:
        build_command();

        meta_t meta() const override;

        // kills any msbuild.exe process, they like to linger and keep file locks
        //
        static void terminate_msbuild();

    protected:
        void convert_cl_to_conf() override;
        clipp::group do_group() override;
        int do_run() override;

    private:
        std::vector<std::string> tasks_;
        bool redownload_  = false;
        bool reextract_   = false;
        bool rebuild_     = false;
        bool reconfigure_ = false;
        bool new_         = false;
        std::optional<bool> clean_;
        std::optional<bool> fetch_;
        std::optional<bool> build_;
        std::optional<bool> nopull_;
        bool ignore_uncommitted_ = false;
        bool keep_msbuild_       = false;
        std::optional<bool> revert_ts_;

        // creates a bare bones ini file in the prefix so mob can be invoked in any
        // directory below it
        //
        void create_prefix_ini();
    };

    // applies a pr
    //
    class pr_command : public command {
    public:
        pr_command();

        meta_t meta() const override;

    protected:
        clipp::group do_group() override;
        std::string do_doc() override;
        int do_run() override;

    private:
        struct pr_info {
            std::string repo, author, branch, title, number;
        };

        std::string op_;
        std::string pr_;
        std::string github_token_;

        std::pair<const tasks::modorganizer*, std::string>
        parse_pr(const std::string& pr) const;

        pr_info get_pr_info(const tasks::modorganizer* task, const std::string& pr);

        std::vector<pr_command::pr_info> get_matching_prs(const std::string& repo_pr);

        std::vector<pr_info> search_prs(const std::string& org,
                                        const std::string& author,
                                        const std::string& branch);

        std::vector<pr_info> validate_prs(const std::vector<pr_info>& prs);

        int pull();
        int find();
        int revert();
    };

    // lists available tasks
    //
    class list_command : public command {
    public:
        meta_t meta() const override;

    protected:
        clipp::group do_group() override;
        int do_run() override;

    private:
        bool all_     = false;
        bool aliases_ = false;
        std::vector<std::string> tasks_;

        void dump(const std::vector<task*>& v, std::size_t indent) const;
        void dump_aliases() const;
    };

    // creates a devbuild or an official release
    //
    class release_command : public command {
    public:
        release_command();
        meta_t meta() const override;

        void make_bin();
        void make_pdbs();
        void make_src();
        void make_uibase();
        void make_installer();

    protected:
        clipp::group do_group() override;
        int do_run() override;
        std::string do_doc() override;
        void convert_cl_to_conf() override;

    private:
        enum class modes { none = 0, devbuild, official };

        modes mode_     = modes::none;
        bool bin_       = true;
        bool src_       = true;
        bool pdbs_      = true;
        bool uibase_    = true;
        bool installer_ = false;
        std::string utf8out_;
        fs::path out_;
        std::string version_;
        bool version_exe_ = false;
        bool version_rc_  = false;
        std::string utf8_rc_path_;
        fs::path rc_path_;
        bool force_ = false;
        std::string suffix_;
        std::string branch_;

        int do_devbuild();
        int do_official();

        void prepare();
        void check_repos_for_branch();
        bool check_clean_prefix();

        fs::path make_filename(const std::string& what) const;

        void walk_dir(const fs::path& dir, std::vector<fs::path>& files,
                      const std::vector<std::regex>& ignore_re,
                      std::size_t& total_size);

        std::string version_from_exe() const;
        std::string version_from_rc() const;
    };

    // manages git repos
    //
    class git_command : public command {
    public:
        git_command();
        meta_t meta() const override;

    protected:
        clipp::group do_group() override;
        int do_run() override;
        std::string do_doc() override;

    private:
        enum class modes { none = 0, set_remotes, add_remote, ignore_ts, branches };

        modes mode_ = modes::none;
        std::string username_;
        std::string email_;
        std::string key_;
        std::string remote_;
        std::string path_;
        bool tson_         = false;
        bool nopush_       = false;
        bool push_default_ = false;
        bool all_branches_ = false;

        void do_set_remotes();
        void do_set_remotes(const fs::path& r);

        void do_add_remote();
        void do_add_remote(const fs::path& r);

        void do_ignore_ts();
        void do_ignore_ts(const fs::path& r);

        void do_branches();

        std::vector<fs::path> get_repos() const;
    };

    // lists the inis found by mob
    //
    class inis_command : public command {
    public:
        meta_t meta() const override;

    protected:
        clipp::group do_group() override;
        int do_run() override;
        std::string do_doc() override;
    };

    // manages transifex
    //
    class tx_command : public command {
    public:
        tx_command();
        meta_t meta() const override;

    protected:
        clipp::group do_group() override;
        void convert_cl_to_conf() override;
        int do_run() override;
        std::string do_doc() override;

    private:
        enum class modes { none = 0, get, build };

        modes mode_ = modes::none;
        std::string key_, team_, project_, url_;
        int min_ = -1;
        std::optional<bool> force_;
        std::string path_;
        std::string dest_;

        void do_get();
        void do_build();
    };

}  // namespace mob

#include "pch.h"
#include "conf.h"
#include "../tasks/task.h"
#include "../tasks/task_manager.h"
#include "../tools/tools.h"
#include "../utility.h"
#include "context.h"
#include "env.h"
#include "ini.h"
#include "paths.h"

namespace mob::details {

    using key_value_map = std::map<std::string, std::string, std::less<>>;
    using section_map   = std::map<std::string, key_value_map, std::less<>>;

    static std::unordered_map<mob::config, std::string_view> s_configuration_values{
        {mob::config::release, "Release"},
        {mob::config::debug, "Debug"},
        {mob::config::relwithdebinfo, "RelWithDebInfo"}};

    // holds all the options not related to tasks (global, tools, paths, etc.)
    static section_map g_conf;

    // holds all the task options; has a special map element with an empty string
    // for options that apply to all tasks, and elements with specific task names
    // for overrides
    static section_map g_tasks;

    // special cases to avoid string manipulations
    static int g_output_log_level = 3;
    static int g_file_log_level   = 5;
    static bool g_dry             = false;

    // check if the two given string are equals case-insensitive
    //
    bool case_insensitive_equals(std::string_view lhs, std::string_view rhs)
    {
        // _strcmpi does not have a n-overload, and since string_view is not
        // necessarily null-terminated, _strmcpi cannot be safely used
        return std::equal(std::begin(lhs), std::end(lhs), std::begin(rhs),
                          std::end(rhs), [](auto&& c1, auto&& c2) {
                              return ::tolower(c1) == ::tolower(c2);
                          });
    }

    bool bool_from_string(std::string_view s)
    {
        return (s == "true" || s == "yes" || s == "1");
    }

    // returns a string from conf, bails out if it doesn't exist
    //
    std::string get_string(std::string_view section, std::string_view key)
    {
        auto sitor = g_conf.find(section);
        if (sitor == g_conf.end())
            gcx().bail_out(context::conf, "[{}] doesn't exist", section);

        auto kitor = sitor->second.find(key);
        if (kitor == sitor->second.end())
            gcx().bail_out(context::conf, "no key '{}' in [{}]", key, section);

        return kitor->second;
    }

    // calls get_string(), converts to int
    //
    int get_int(std::string_view section, std::string_view key)
    {
        const auto s = get_string(section, key);

        try {
            return std::stoi(s);
        }
        catch (std::exception&) {
            gcx().bail_out(context::conf, "bad int for {}/{}", section, key);
        }
    }

    // calls get_string(), converts to bool
    //
    bool get_bool(std::string_view section, std::string_view key)
    {
        const auto s = get_string(section, key);
        return bool_from_string(s);
    }

    // sets the given option, bails out if the option doesn't exist
    //
    void set_string(std::string_view section, std::string_view key,
                    std::string_view value)
    {
        auto sitor = g_conf.find(section);
        if (sitor == g_conf.end())
            gcx().bail_out(context::conf, "[{}] doesn't exist", section);

        auto kitor = sitor->second.find(key);
        if (kitor == sitor->second.end())
            gcx().bail_out(context::conf, "no key '{}' [{}]", key, section);

        kitor->second = value;
    }

    config string_to_config(std::string_view value)
    {
        for (const auto& [c, v] : s_configuration_values) {
            if (case_insensitive_equals(value, v)) {
                return c;
            }
        }

        gcx().bail_out(context::conf, "invalid configuration '{}'", value);
    }

    // sets the given option, adds it if it doesn't exist; used when setting options
    // from the master ini
    //
    void add_string(const std::string& section, const std::string& key,
                    std::string value)
    {
        g_conf[section][key] = value;
    }

    // finds an option for the given task, returns empty if not found
    //
    std::optional<std::string> find_string_for_task(std::string_view task_name,
                                                    std::string_view key)
    {
        // find task
        auto titor = g_tasks.find(task_name);
        if (titor == g_tasks.end())
            return {};

        const auto& task = titor->second;

        // find key
        auto itor = task.find(key);
        if (itor == task.end())
            return {};

        return itor->second;
    }

    // gets an option for any of the given task names, typically what task::names()
    // returns, which contains the main task name plus some alternate names
    //
    // there's a hierarchy for task options:
    //
    //  1) there's a special "_override" entry in g_tasks, for options set from
    //     the command line that should override everything, like --no-pull
    //     should override all pull settings for all tasks
    //
    //  2) if the key is not found in "_override", then there can be an entry
    //     in g_tasks with any of given task names
    //
    //  3) if the key doesn't exist, then use the generic task option for it, stored
    //     in an element with an empty string in g_tasks
    //
    std::string get_string_for_task(const std::vector<std::string>& task_names,
                                    std::string_view key)
    {
        // some command line options will override any user settings, like
        // --no-pull, those are stored in a special _override task name
        auto v = find_string_for_task("_override", key);
        if (v)
            return *v;

        // look for an option for this task by name
        for (auto&& tn : task_names) {
            v = find_string_for_task(tn, key);
            if (v)
                return *v;
        }

        // default task options are in a special empty string entry in g_tasks
        v = find_string_for_task("", key);
        if (v)
            return *v;

        // doesn't exist anywhere
        gcx().bail_out(context::conf, "no task option '{}' found for any of {}", key,
                       join(task_names, ","));
    }

    // calls get_string_for_task(), converts to bool
    //
    bool get_bool_for_task(const std::vector<std::string>& task_names,
                           std::string_view key)
    {
        const std::string s = get_string_for_task(task_names, key);
        return bool_from_string(s);
    }

    // sets the given task option, bails out if the option doesn't exist
    //
    void set_string_for_task(const std::string& task_name, const std::string& key,
                             std::string value)
    {
        // make sure the key exists, will throw if it doesn't
        get_string_for_task({task_name}, key);

        g_tasks[task_name][key] = std::move(value);
    }

    // sets the given task option, adds it if it doesn't exist; used when setting
    // options from the master ini
    //
    void add_string_for_task(const std::string& task_name, const std::string& key,
                             std::string value)
    {
        g_tasks[task_name][key] = std::move(value);
    }

    // read a CMake constant from the configuration
    //
    template <typename T>
    T parse_cmake_value(std::string_view section, std::string_view key,
                        std::string_view value,
                        std::unordered_map<T, std::string_view> const& values)
    {
        for (const auto& [value_c, value_s] : values) {
            if (case_insensitive_equals(value_s, value)) {
                return value_c;
            }
        }

        // build a string containing allowed value for logging
        std::vector<std::string_view> values_s;
        for (const auto& [value_c, value_s] : values) {
            values_s.push_back(value_s);
        }
        gcx().bail_out(context::conf, "bad value '{}' for {}/{} (expected one of {})",
                       value, section, key, join(values_s, ", ", std::string{}));
    }

}  // namespace mob::details

namespace mob {

    std::vector<std::string> format_options()
    {
        // don't log private stuff
        auto hide = [](std::string_view section, std::string_view key) {
            if (key == "github_key")
                return true;

            if (section == "transifex" && key == "key")
                return true;

            if (key == "git_email")
                return true;

            return false;
        };

        auto make_value = [&](auto&& s, auto&& k, auto&& v) -> std::string {
            if (hide(s, k))
                return v.empty() ? "" : "(hidden)";
            else
                return v;
        };

        auto& tm = task_manager::instance();

        std::size_t longest_what = 0;
        std::size_t longest_key  = 0;

        for (auto&& [section, kvs] : details::g_conf) {
            longest_what = std::max(longest_what, section.size());

            for (auto&& [k, v] : kvs)
                longest_key = std::max(longest_key, k.size());
        }

        for (auto&& [k, v] : details::g_tasks[""])
            longest_key = std::max(longest_key, k.size());

        for (const auto* task : tm.all())
            longest_what = std::max(longest_what, task->name().size());

        std::vector<std::string> lines;

        lines.push_back(pad_right("what", longest_what) + "  " +
                        pad_right("key", longest_key) + "   " + "value");

        lines.push_back(pad_right("-", longest_what, '-') + "  " +
                        pad_right("-", longest_key, '-') + "   " + "-----");

        for (auto&& [section, kvs] : details::g_conf) {
            for (auto&& [k, v] : kvs) {
                lines.push_back(pad_right(section, longest_what) + "  " +
                                pad_right(k, longest_key) + " = " +
                                make_value(section, k, v));
            }
        }

        for (auto&& [k, v] : details::g_tasks[""]) {
            lines.push_back(pad_right("task", longest_what) + "  " +
                            pad_right(k, longest_key) + " = " + make_value("", k, v));
        }

        for (const auto* t : tm.all()) {
            for (auto&& [k, unused] : details::g_tasks[""]) {
                lines.push_back(
                    pad_right(t->name(), longest_what) + "  " +
                    pad_right(k, longest_key) + " = " +
                    make_value("", k, details::get_string_for_task({t->name()}, k)));
            }
        }

        return lines;
    }

    // sets commonly used options that need to be converted to int/bool, for
    // performance
    //
    void set_special_options()
    {
        details::g_output_log_level = details::get_int("global", "output_log_level");
        details::g_file_log_level   = details::get_int("global", "file_log_level");
        details::g_dry              = details::get_bool("global", "dry");
    }

    // sets an option `key` in the `paths` section; if the path is currently empty,
    // sets it using `f` (which is either a callable or a string)
    //
    // in any case, makes it absolute and canonical, bails out if the path does not
    // exist
    //
    // this is used for paths that should already exist (qt, vs, etc.)
    //
    template <class F>
    void set_path_if_empty(std::string_view key, F&& f)
    {
        // current value
        fs::path p = conf().path().get(key);

        if (p.empty()) {
            // empty, set it from `f`
            if constexpr (std::is_same_v<fs::path, std::decay_t<decltype(f)>>)
                p = f;
            else
                p = f();
        }

        p = fs::absolute(p);

        if (!conf().global().dry()) {
            if (!fs::exists(p))
                gcx().bail_out(context::conf, "path {} not found", p);

            p = fs::canonical(p);
        }

        // new value
        details::set_string("paths", key, path_to_utf8(p));
    }

    // sets an option `key` in the `paths` section:
    //   - if the path is empty, sets it as default_parent/default_dir,
    //   - if the path is not empty but is relative, resolves it against
    //     default_parent
    //
    // in any case, makes it absolute but weakly canonical since it might not exist
    // at that point (this is used for build, install, etc.)
    //
    void resolve_path(std::string_view key, const fs::path& default_parent,
                      std::string_view default_dir)
    {
        // current value
        fs::path p = conf().path().get(key);

        if (p.empty()) {
            p = default_parent / default_dir;
        }
        else {
            if (p.is_relative())
                p = default_parent / p;
        }

        if (!conf().global().dry())
            p = fs::weakly_canonical(fs::absolute(p));

        details::set_string("paths", key, path_to_utf8(p));
    }

    // `section_string` can be something like "global" or "paths", but also "task"
    // or a task-specific name like "uibase:task"
    //
    // `master` is true if the ini being processed is the master ini so options are
    // added to the maps instead of set, which throws if they're not found
    //
    void process_option(const std::string& section_string, const std::string& key,
                        const std::string& value, bool master)
    {
        // split section string on ":"
        const auto col = section_string.find(":");
        std::string task, section;

        if (col == std::string::npos) {
            // not a "task_name:task" section
            section = section_string;
        }
        else {
            // that's a "task_name:task" section
            task    = section_string.substr(0, col);
            section = section_string.substr(col + 1);
        }

        if (section == "task") {
            // task options go in g_tasks

            if (task == "_override") {
                // special case, comes from options on the command line
                details::set_string_for_task("_override", key, value);
            }
            else if (task != "") {
                // task specific

                // task must exist
                const auto& tasks = task_manager::instance().find(task);

                if (tasks.empty()) {
                    gcx().bail_out(context::conf, "bad option {}, task '{}' not found",
                                   section_string, task);
                }

                MOB_ASSERT(!tasks.empty());

                for (auto& t : tasks) {
                    if (t->name() != task &&
                        details::find_string_for_task(t->name(), key)) {
                        continue;
                    }
                    details::set_string_for_task(t->name(), key, value);
                }
            }
            else {
                // global task option

                if (master)
                    details::add_string_for_task("", key, value);
                else
                    details::set_string_for_task("", key, value);
            }
        }
        else {
            // not a task option, goes into g_conf

            if (master)
                details::add_string(section, key, value);
            else
                details::set_string(section, key, value);
        }
    }

    // reads the given ini and adds all of its content to the options
    //
    void process_ini(const fs::path& ini, bool master)
    {
        const auto data = parse_ini(ini);

        for (auto&& a : data.aliases)
            task_manager::instance().add_alias(a.first, a.second);

        for (auto&& [section_string, kvs] : data.sections) {
            for (auto&& [k, v] : kvs)
                process_option(section_string, k, v, master);
        }
    }

    // parses the given option strings and adds them as options
    //
    void process_cmd_options(const std::vector<std::string>& opts)
    {
        // parses "section/key=value"
        static std::regex re(R"((.+)/(.+)=(.*))");

        gcx().debug(context::conf, "overriding from command line:");

        for (auto&& o : opts) {
            std::smatch m;
            if (!std::regex_match(o, m, re)) {
                gcx().bail_out(context::conf,
                               "bad option {}, must be [task:]section/key=value", o);
            }

            process_option(m[1], m[2], m[3], false);
        }
    }

    // goes through all the options that have to do with paths, checks them and
    // resolves them if necessary
    //
    void resolve_paths()
    {
        // first, if any of these paths are empty, they are set using the second
        // argument, which can be callable or a path
        //
        // the resulting path is made absolute and canonical and will bail out if it
        // doesn't exist

        // make sure third-party is in PATH before the other paths are checked
        // because some of these paths will need to look in there to find stuff
        set_path_if_empty("third_party", find_third_party_directory);
        this_env::prepend_to_path(conf().path().third_party() / "bin");

        set_path_if_empty("pf_x86", find_program_files_x86);
        set_path_if_empty("pf_x64", find_program_files_x64);
        set_path_if_empty("vs", find_vs);
        set_path_if_empty("vcpkg", find_vcpkg);  // set after vs as it will use the VS
        set_path_if_empty("qt_install", find_qt);
        set_path_if_empty("temp_dir", find_temp_dir);
        set_path_if_empty("patches", find_in_root("patches"));
        set_path_if_empty("licenses", find_in_root("licenses"));
        set_path_if_empty("qt_bin", qt::installation_path() / "bin");
        set_path_if_empty("qt_translations", qt::installation_path() / "translations");

        // second, if any of these paths are relative, they use the second argument
        // as the root; if they're empty, they combine the second and third
        // arguments
        //
        // these paths might not exist yet, so they're only made weakly canonical,
        // they'll be created as needed during the build process

        const auto p = conf().path();

        resolve_path("cache", p.prefix(), "downloads");
        resolve_path("build", p.prefix(), "build");
        resolve_path("install", p.prefix(), "install");
        resolve_path("install_installer", p.install(), "installer");
        resolve_path("install_bin", p.install(), "bin");
        resolve_path("install_libs", p.install(), "lib");
        resolve_path("install_pdbs", p.install(), "pdb");
        resolve_path("install_dlls", p.install_bin(), "dlls");
        resolve_path("install_loot", p.install_bin(), "loot");
        resolve_path("install_plugins", p.install_bin(), "plugins");
        resolve_path("install_licenses", p.install_bin(), "licenses");
        resolve_path("install_pythoncore", p.install_bin(), "pythoncore");
        resolve_path("install_stylesheets", p.install_bin(), "stylesheets");
        resolve_path("install_translations", p.install_bin(), "translations");

        // finally, resolve the tools that are unlikely to be in PATH; all the
        // other tools (7z, jom, patch, etc.) are assumed to be in PATH (which
        // now contains third-party) or have valid absolute paths in the ini

        details::set_string("tools", "vcvars", path_to_utf8(find_vcvars()));
        details::set_string("tools", "iscc", path_to_utf8(find_iscc()));
    }

    void conf::set_log_file()
    {
        // set up the log file, resolve against prefix if relative
        fs::path log_file = conf().global().get("log_file");
        if (log_file.is_relative())
            log_file = conf().path().prefix() / log_file;

        context::set_log_file(log_file);
    }

    void init_options(const std::vector<fs::path>& inis,
                      const std::vector<std::string>& opts)
    {
        MOB_ASSERT(!inis.empty());

        // some logging
        gcx().debug(context::conf, "cl: {}", std::wstring(GetCommandLineW()));
        gcx().debug(context::conf, "using inis in order:");
        for (auto&& ini : inis)
            gcx().debug(context::conf, "  . {}", ini);

        // used to resolve a relative prefix; by default, it's resolved against cwd,
        // but if an ini other than the master contains a prefix, use the ini's
        // parent directory instead
        fs::path prefix_root = fs::current_path();

        // true for the first ini, will add values to the configuration maps instead
        // of setting them, which throws if the option doesn't exist
        //
        // the goal is that the first, master ini contains all existing options and
        // if an option  set in another ini or on the command line doesn't exist in
        // the master, it's an error
        bool master = true;

        for (auto&& ini : inis) {
            fs::path prefix_before;

            // if this is the master ini, the prefix doesn't exist in the config
            // yet because no inis have been loaded
            if (!master)
                prefix_before = conf().path().prefix();

            process_ini(ini, master);

            // check if the prefix was changed by this ini
            if (!master && conf().path().prefix() != prefix_before) {
                // remember its path
                prefix_root = ini.parent_path();
            }

            // further inis should only contain options that already exist
            master = false;
        }

        if (!opts.empty()) {
            const fs::path prefix_before = conf().path().prefix();

            process_cmd_options(opts);

            // check if the prefix was changed on the command line
            if (conf().path().prefix() != prefix_before) {
                // use cwd as the parent of a relative prefix
                prefix_root = fs::current_path();
            }
        }

        // converts some options to ints or bools, these are used everywhere, like
        // the log levels
        set_special_options();

        // an empty prefix is an error and will fail in validate_options(), but
        // don't check it here to allow some commands to run, like `mob options`,
        // and make sure it's not set to something that's not empty to make sure it
        // _does_ fail later on
        if (!conf().path().prefix().empty())
            resolve_path("prefix", prefix_root, "");

        // set up the log file, resolve against prefix if relative
        conf().set_log_file();

        // goes through all paths and tools, finds missing or relative stuff, bails
        // out of stuff can't be found
        resolve_paths();

        // make sure qt's bin directory is in the path
        this_env::append_to_path(conf().path().get("qt_bin"));
    }

    bool verify_options()
    {
        // can't have an empty prefix
        if (conf().path().prefix().empty()) {
            u8cerr << "missing prefix; either specify it the [paths] section of "
                   << "the ini or pass '-d path'\n";

            return false;
        }

        // don't build mo inside mob
        if (fs::exists(conf().path().prefix())) {
            if (fs::equivalent(conf().path().prefix(), mob_exe_path().parent_path())) {
                u8cerr << "the prefix cannot be where mob.exe is, there's already a "
                       << "build directory in there\n";

                return false;
            }
        }

        return true;
    }

    conf_global conf::global()
    {
        return {};
    }

    conf_task conf::task(const std::vector<std::string>& names)
    {
        return {names};
    }

    conf_cmake conf::cmake()
    {
        return {};
    }

    conf_tools conf::tool()
    {
        return {};
    }

    conf_transifex conf::transifex()
    {
        return {};
    }

    conf_prebuilt conf::prebuilt()
    {
        return {};
    }

    conf_versions conf::version()
    {
        return {};
    }

    conf_build_types conf::build_types()
    {
        return {};
    }

    conf_paths conf::path()
    {
        return {};
    }

    conf_global::conf_global() : conf_section("global") {}

    int conf_global::output_log_level() const
    {
        return details::g_output_log_level;
    }

    int conf_global::file_log_level() const
    {
        return details::g_file_log_level;
    }

    bool conf_global::dry() const
    {
        return details::g_dry;
    }

    // use appropriate case for the below constants since we will be using them in
    // to_string, although most of cmake and msbuild is case-insensitive so it will
    // not matter much in the end

    static std::unordered_map<conf_cmake::constant, std::string_view> constant_values{
        {conf_cmake::always, "ALWAYS"},
        {conf_cmake::lazy, "LAZY"},
        {conf_cmake::never, "NEVER"}};

    std::string conf_cmake::to_string(constant c)
    {
        return std::string{constant_values.at(c)};
    }

    conf_cmake::conf_cmake() : conf_section("cmake") {}

    conf_cmake::constant conf_cmake::install_message() const
    {
        return details::parse_cmake_value(
            name(), "install_message", details::get_string(name(), "install_message"),
            constant_values);
    }

    std::string conf_cmake::host() const
    {
        return details::get_string(name(), "host");
    }

    conf_task::conf_task(std::vector<std::string> names) : names_(std::move(names)) {}

    std::string conf_task::get(std::string_view key) const
    {
        return details::get_string_for_task(names_, key);
    }

    bool conf_task::get_bool(std::string_view key) const
    {
        return details::get_bool_for_task(names_, key);
    }

    mob::config conf_task::configuration() const
    {
        return details::parse_cmake_value(
            names_[0], "configuration",
            details::get_string_for_task(names_, "configuration"),
            details::s_configuration_values);
    }

    conf_tools::conf_tools() : conf_section("tools") {}

    conf_transifex::conf_transifex() : conf_section("transifex") {}

    conf_versions::conf_versions() : conf_section("versions") {}

    conf_build_types::conf_build_types() : conf_section("build-types") {}

    conf_prebuilt::conf_prebuilt() : conf_section("prebuilt") {}

    conf_paths::conf_paths() : conf_section("paths") {}

}  // namespace mob

#include "pch.h"
#include "task.h"
#include "../core/conf.h"
#include "../core/op.h"
#include "../tools/tools.h"
#include "../utility/threading.h"
#include "task_manager.h"

namespace mob {

    // converts the given flag to a string
    //
    std::string to_string(task::clean c)
    {
        // for warnings
        switch (c) {
        case task::clean::nothing:
            break;
        case task::clean::redownload:
            break;
        case task::clean::reextract:
            break;
        case task::clean::reconfigure:
            break;
        case task::clean::rebuild:
            break;
        }

        std::vector<std::string> v;

        if (is_set(c, task::clean::redownload))
            v.push_back("redownload");

        if (is_set(c, task::clean::reextract))
            v.push_back("reextract");

        if (is_set(c, task::clean::reconfigure))
            v.push_back("reconfigure");

        if (is_set(c, task::clean::rebuild))
            v.push_back("rebuild");

        return join(v, "|");
    }

    // combines the clean flags depending on the configuration
    //
    task::clean make_clean_flags()
    {
        task::clean c = task::clean::nothing;
        const auto g  = conf().global();

        if (g.redownload())
            c |= task::clean::redownload;

        if (g.reextract())
            c |= task::clean::reextract;

        if (g.reconfigure())
            c |= task::clean::reconfigure;

        if (g.rebuild())
            c |= task::clean::rebuild;

        return c;
    }

    task::task(std::vector<std::string> names)
        : names_(std::move(names)), bailed_(), interrupted_(false)
    {
        // make sure there's a context to return in cx() for the thread that created
        // this task, there's a bunch of places where tasks need to log things
        // before a thread is created
        add_context_for_this_thread(name());

        // don't register parallel tasks so they're not shown to the user, they're
        // useless
        if (name() != "parallel")
            task_manager::instance().register_task(this);
    }

    // anchor
    task::~task() = default;

    bool task::enabled() const
    {
        return conf().task(names()).get<bool>("enabled");
    }

    void task::do_clean(clean)
    {
        // no-op
    }

    void task::do_fetch()
    {
        // no-op
    }

    void task::do_build_and_install()
    {
        // no-op
    }

    context& task::cx()
    {
        return const_cast<context&>(std::as_const(*this).cx());
    }

    const context& task::cx() const
    {
        static context bad("?");

        const auto tid = std::this_thread::get_id();

        {
            std::scoped_lock lock(contexts_mutex_);

            auto itor = contexts_.find(tid);
            if (itor != contexts_.end())
                return *itor->second;
        }

        return bad;
    }

    const std::string& task::name() const
    {
        return names_[0];
    }

    const std::vector<std::string>& task::names() const
    {
        return names_;
    }

    bool task::name_matches(std::string_view pattern) const
    {
        if (pattern.find('*') != std::string::npos)
            return name_matches_glob(pattern);
        else
            return name_matches_string(pattern);
    }

    bool task::name_matches_glob(std::string_view pattern) const
    {
        try {
            // converts '*' to '.*', changes underscores to dashes so they're
            // equivalent, then matches the pattern as a regex, case insensitive

            std::string fixed_pattern(pattern);
            fixed_pattern = replace_all(fixed_pattern, "*", ".*");
            fixed_pattern = replace_all(fixed_pattern, "_", "-");

            std::regex re(fixed_pattern, std::regex::icase);

            for (auto&& n : names_) {
                const std::string fixed_name(replace_all(n, "_", "-"));

                if (std::regex_match(fixed_name, re))
                    return true;
            }

            return false;
        }
        catch (std::exception&) {
            u8cerr << "bad glob '" << pattern << "'\n"
                   << "globs are actually bastardized regexes where '*' is "
                   << "replaced by '.*', so don't push it\n";

            throw bailed();
        }
    }

    bool task::name_matches_string(std::string_view pattern) const
    {
        for (auto&& n : names_) {
            if (strings_match(n, pattern))
                return true;
        }

        return false;
    }

    bool task::strings_match(std::string_view a, std::string_view b) const
    {
        // this is actually called a crapload of times and is worth the
        // optimization, especially for debug builds

        if (a.size() != b.size())
            return false;

        for (std::size_t i = 0; i < a.size(); ++i) {
            // underscores and dashes are equivalent
            if ((a[i] == '-' || a[i] == '_') && (b[i] == '-' || b[i] == '_'))
                continue;

            // case insensitive comparison
            const auto ac = static_cast<unsigned char>(a[i]);
            const auto bc = static_cast<unsigned char>(b[i]);

            if (std::tolower(ac) != std::tolower(bc))
                return false;
        }

        return true;
    }

    void task::add_context_for_this_thread(std::string name)
    {
        std::scoped_lock lock(contexts_mutex_);

        const auto tid = std::this_thread::get_id();

        // there might already be a context for this thread, such as when run()
        // is called, because it's typically called from the same thread as the
        // one that created the task, and a context is added in the task's
        // constructor
        //
        // but run() can also be called from parallel_tasks in a thread, so make
        // sure there's a context for it

        auto itor = contexts_.find(tid);
        if (itor == contexts_.end())
            contexts_.emplace(tid, std::make_unique<context>(std::move(name)));
    }

    void task::remove_context_for_this_thread()
    {
        std::scoped_lock lock(contexts_mutex_);

        // removes the context for the current thread

        const auto tid = std::this_thread::get_id();

        auto itor = contexts_.find(tid);
        if (itor != contexts_.end())
            contexts_.erase(itor);
    }

    void task::running_from_thread(std::string thread_name, std::function<void()> f)
    {
        try {
            // make sure there's a context for this thread for the duration of f()
            add_context_for_this_thread(thread_name);
            guard g([&] {
                remove_context_for_this_thread();
            });

            f();
        }
        catch (bailed e) {
            // something in f() bailed out, interrupt everything

            bailed_ = e;

            gcx().error(context::generic, "{} bailed out, interrupting all tasks",
                        name());

            task_manager::instance().interrupt_all();
        }
        catch (interrupted) {
            // this task was interrupted, just quit
            return;
        }
    }

    void task::parallel(parallel_functions v, std::optional<std::size_t> threads)
    {
        thread_pool tp(threads);

        for (auto&& [name, f] : v) {
            cx().trace(context::generic, "running in parallel: {}", name);

            tp.add([this, name, f] {
                running_from_thread(name, f);
            });
        }
    }

    conf_task task::task_conf() const
    {
        return conf().task(names());
    }

    git task::make_git() const
    {
        // always either clone or pull depending on whether the repo is already
        // there, unless --no-pull is given
        const auto o = task_conf().no_pull() ? git::clone : git::clone_or_pull;

        git g(o);

        // set up the git tool with the task's settings
        g.ignore_ts_on_clone(task_conf().ignore_ts());
        g.revert_ts_on_pull(task_conf().revert_ts());
        g.credentials(task_conf().git_user(), task_conf().git_email());
        g.shallow(task_conf().git_shallow());

        if (task_conf().set_origin_remote()) {
            g.remote(task_conf().remote_org(), task_conf().remote_key(),
                     task_conf().remote_no_push_upstream(),
                     task_conf().remote_push_default_origin());
        }

        return g;
    }

    std::string task::make_git_url(const std::string& org,
                                   const std::string& repo) const
    {
        return task_conf().git_url_prefix() + org + "/" + repo + ".git";
    }

    fs::path task::get_source_path() const
    {
        return {};
    }

    bool task::get_prebuilt() const
    {
        return false;
    }

    void task::run()
    {
        // make sure there's a context for this thread; run() can be called from
        // the main thread or from parallel_tasks, for example, so it might be in
        // a new thread or not
        running_from_thread(name(), [&] {
            if (!enabled()) {
                cx().debug(context::generic, "task is disabled");
                return;
            }

            cx().info(context::generic, "running task");

            // clean task if needed
            clean_task();
            check_interrupted();

            // fetch task if needed
            fetch();
            check_interrupted();

            // build/install if needed
            build_and_install();
            check_interrupted();
        });
    }

    void task::interrupt()
    {
        std::scoped_lock lock(tools_mutex_);

        interrupted_ = true;

        for (auto* t : tools_)
            t->interrupt();
    }

    void task::clean_task()
    {
        if (!conf().global().clean())
            return;

        if (!enabled()) {
            cx().debug(context::generic, "cleaning (skipping, task disabled)");
            return;
        }

        const auto cf = make_clean_flags();

        if (cf != clean::nothing) {
            cx().info(context::rebuild, "cleaning ({})", to_string(cf));
            do_clean(cf);
        }
    }

    void task::fetch()
    {
        if (!conf().global().fetch())
            return;

        if (!enabled()) {
            cx().debug(context::generic, "fetching (skipping, task disabled)");
            return;
        }

        cx().info(context::generic, "fetching");

        do_fetch();
        check_interrupted();

        // auto patching if the task has a source path
        if (!get_source_path().empty()) {
            cx().debug(context::generic, "patching");

            run_tool(patcher().task(name(), get_prebuilt()).root(get_source_path()));
        }
    }

    void task::build_and_install()
    {
        if (!conf().global().build())
            return;

        if (!enabled()) {
            cx().debug(context::generic, "build and install (skipping, task disabled)");

            return;
        }

        cx().info(context::generic, "build and install");
        do_build_and_install();
        cx().info(context::generic, "done");
    }

    void task::check_bailed()
    {
        if (bailed_)
            throw *bailed_;
    }

    void task::check_interrupted()
    {
        if (interrupted_)
            throw interrupted();
    }

    void task::run_tool_impl(tool* t)
    {
        {
            // add tool to list so it can be interrupted
            std::scoped_lock lock(tools_mutex_);
            tools_.push_back(t);
        }

        guard g([&] {
            // pop the tool
            std::scoped_lock lock(tools_mutex_);
            std::erase(tools_, t);
        });

        cx().debug(context::generic, "running tool {}", t->name());

        check_interrupted();
        t->run(cx());
        check_interrupted();
    }

    parallel_tasks::parallel_tasks() : task("parallel") {}

    parallel_tasks::~parallel_tasks()
    {
        join();
    }

    bool parallel_tasks::enabled() const
    {
        // can't disable parallel tasks
        return true;
    }

    void parallel_tasks::add_task(std::unique_ptr<task> t)
    {
        children_.push_back(std::move(t));
    }

    std::vector<task*> parallel_tasks::children() const
    {
        std::vector<task*> v;

        for (auto&& t : children_)
            v.push_back(t.get());

        return v;
    }

    void parallel_tasks::run()
    {
        // creates a thread for each child and calls run()
        for (auto& t : children_)
            threads_.push_back(start_thread([&] {
                t->run();
            }));

        join();
    }

    void parallel_tasks::interrupt()
    {
        for (auto& t : children_)
            t->interrupt();
    }

    void parallel_tasks::check_bailed()
    {
        for (auto& t : children_)
            t->check_bailed();
    }

    void parallel_tasks::join()
    {
        for (auto& t : threads_)
            t.join();

        threads_.clear();
    }

}  // namespace mob

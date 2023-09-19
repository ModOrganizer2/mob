#include "pch.h"
#include "tasks.h"

namespace mob::tasks {

    // boost-di is needed by bsapacker

    boost_di::boost_di() : basic_task("boost-di", "boostdi", "boost_di") {}

    std::string boost_di::version()
    {
        return {};
    }

    bool boost_di::prebuilt()
    {
        // prebuilts don't exist for this, it's headers only
        return false;
    }

    fs::path boost_di::source_path()
    {
        return conf().path().build() / "di";
    }

    void boost_di::do_clean(clean c)
    {
        // delete the whole thing
        if (is_set(c, clean::reclone))
            git_wrap::delete_directory(cx(), source_path());
    }

    void boost_di::do_fetch()
    {
        run_tool(make_git()
                     .url(make_git_url("boost-experimental", "di"))
                     .branch("cpp14")
                     .root(source_path()));
    }

}  // namespace mob::tasks

#include "pch.h"
#include "../core/conf.h"
#include "../core/context.h"
#include "../core/env.h"
#include "../net.h"
#include "../tasks/tasks.h"
#include "../utility.h"
#include "commands.h"

namespace mob {

    tx_command::tx_command() : command(requires_options) {}

    command::meta_t tx_command::meta() const
    {
        return {"tx", "manages transifex translations"};
    }

    clipp::group tx_command::do_group()
    {
        return clipp::group(
            clipp::command("tx").set(picked_),

            (clipp::option("-h", "--help") >> help_) % ("shows this message"),

            "get" % (clipp::command("get").set(mode_, modes::get),
                     (clipp::option("-k", "--key") & clipp::value("APIKEY") >> key_) %
                         "API key",

                     (clipp::option("-t", "--team") & clipp::value("TEAM") >> team_) %
                         "team name",

                     (clipp::option("-p", "--project") &
                      clipp::value("PROJECT") >> project_) %
                         "project name",

                     (clipp::option("-u", "--url") & clipp::value("URL") >> url_) %
                         "project URL",

                     (clipp::option("-m", "--minimum") &
                      clipp::value("PERCENT").set(min_)) %
                         "minimum translation threshold to download [0-100]",

                     (clipp::option("-f", "--force").call([&] {
                         force_ = true;
                     })) %
                         "don't check timestamps, re-download all translation files",

                     (clipp::value("path") >> path_) %
                         "path that will contain the .tx directory")

                |

                "build" % (clipp::command("build").set(mode_, modes::build),

                           (clipp::value("source") >> path_) %
                               "path that contains the translation directories",

                           (clipp::value("destination") >> dest_) %
                               "path that will contain the .qm files"));
    }

    void tx_command::convert_cl_to_conf()
    {
        command::convert_cl_to_conf();

        if (!key_.empty())
            common.options.push_back("transifex/key=" + key_);

        if (!team_.empty())
            common.options.push_back("transifex/team=" + team_);

        if (!project_.empty())
            common.options.push_back("transifex/project=" + project_);

        if (!url_.empty())
            common.options.push_back("transifex/url=" + url_);

        if (min_ >= 0)
            common.options.push_back("transifex/minimum=" + std::to_string(min_));

        if (force_)
            common.options.push_back("transifex/force=" + std::to_string(*force_));
    }

    int tx_command::do_run()
    {
        switch (mode_) {
        case modes::get:
            do_get();
            break;

        case modes::build:
            do_build();
            break;

        case modes::none:
        default:
            u8cerr << "bad tx mode " << static_cast<int>(mode_) << "\n";
            throw bailed();
        }

        return 0;
    }

    std::string tx_command::do_doc()
    {
        return "Some values will be taken from the INI file if not specified.\n"
               "\n"
               "Commands:\n"
               "get\n"
               "  Initializes a Transifex project in the given directory if\n"
               "  necessary and pulls all the translation files.\n"
               "\n"
               "build\n"
               "  Builds all .qm files. The path can either be the transifex\n"
               "  project (where .tx is) or the `translations` directory (where the\n"
               "  individual translation directories are).";
    }

    void tx_command::do_get()
    {
        const url u = conf().transifex().get("url") + "/" +
                      conf().transifex().get("team") + "/" +
                      conf().transifex().get("project");

        const std::string key = conf().transifex().get("key");

        if (key.empty() && !this_env::get_opt("TX_TOKEN")) {
            u8cout << "(no key was in the INI, --key wasn't given and TX_TOKEN env\n"
                      "variable doesn't exist, this will probably fail)\n\n";
        }

        // copy the global context, the tools will modify it
        context cxcopy = gcx();

        u8cout << "initializing\n";
        transifex(transifex::init).root(path_).run(cxcopy);

        u8cout << "configuring\n";
        transifex(transifex::config)
            .stdout_level(context::level::info)
            .root(path_)
            .api_key(key)
            .url(u)
            .run(cxcopy);

        u8cout << "pulling\n";
        transifex(transifex::pull)
            .stdout_level(context::level::info)
            .root(path_)
            .api_key(key)
            .minimum(conf().transifex().get<int>("minimum"))
            .force(conf().transifex().get<bool>("force"))
            .run(cxcopy);
    }

    void tx_command::do_build()
    {
        fs::path root = path_;
        if (fs::exists(root / ".tx") && fs::exists(root / "translations"))
            root = root / "translations";

        tasks::translations::projects ps(root);

        fs::path dest = dest_;
        op::create_directories(gcx(), dest, op::unsafe);

        for (auto&& w : ps.warnings())
            u8cerr << w << "\n";

        thread_pool tp;

        for (auto& p : ps.get()) {
            for (auto& lg : p.langs) {
                // copy the global context, each thread must have its own
                tp.add([&, cxcopy = gcx()]() mutable {
                    lrelease()
                        .project(p.name)
                        .sources(lg.ts_files)
                        .out(dest)
                        .run(cxcopy);
                });
            }
        }
    }

}  // namespace mob

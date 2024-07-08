#include "pch.h"
#include "cmd/commands.h"
#include "core/conf.h"
#include "core/op.h"
#include "net.h"
#include "tasks/task_manager.h"
#include "tasks/tasks.h"
#include "tools/tools.h"
#include "utility.h"
#include "utility/threading.h"

namespace mob {

    void add_tasks()
    {
        using namespace tasks;

        // add new tasks here
        //
        // top level tasks are run sequentially, tasks added to a parallel_tasks will
        // run in parallel; which tasks are run in parallel is somewhat arbitrary when
        // there's no dependency, the goal is just to saturate the cpu
        //
        // mob doesn't have a concept of task dependencies, just task ordering, so
        // if a task depends on another, it has to be earlier in the order

        // third-party tasks

        // add_task<parallel_tasks>()
        //     .add_task<sevenz>()
        //     .add_task<zlib>()
        //     .add_task<gtest>()
        //     .add_task<libbsarch>()
        //     .add_task<libloot>()
        //     .add_task<openssl>()
        //     .add_task<bzip2>()
        //     .add_task<directxtex>();

        // add_task<parallel_tasks>()
        //     .add_task<tasks::python>()
        //     .add_task<lz4>()
        //     .add_task<spdlog>();

        // add_task<parallel_tasks>()
        //     .add_task<boost>()
        //     .add_task<boost_di>()
        //     .add_task<sip>();

        // add_task<parallel_tasks>()
        //     .add_task<pyqt>()
        //     .add_task<pybind11>()
        //     .add_task<usvfs>()
        //     .add_task<stylesheets>()
        //     .add_task<licenses>()
        //     .add_task<explorerpp>();

        // super tasks

        using mo = modorganizer;

        // most of the alternate names below are from the transifex slugs, which
        // are sometimes different from the project names, for whatever reason

        add_task<parallel_tasks>().add_task<usvfs>().add_task<mo>("cmake_common");

        add_task<mo>("modorganizer-uibase");

        add_task<parallel_tasks>()
            .add_task<mo>("modorganizer-archive")
            .add_task<mo>("modorganizer-lootcli")
            .add_task<mo>("modorganizer-esptk")
            .add_task<mo>("modorganizer-bsatk")
            .add_task<mo>("modorganizer-nxmhandler")
            .add_task<mo>("modorganizer-helper")
            .add_task<mo>({"modorganizer-bsapacker", "bsa_packer"})
            .add_task<mo>("modorganizer-preview_bsa")
            .add_task<mo>("modorganizer-game_bethesda");

        add_task<parallel_tasks>()
            .add_task<mo>({"modorganizer-tool_inieditor", "inieditor"})
            .add_task<mo>({"modorganizer-tool_inibakery", "inibakery"})
            .add_task<mo>("modorganizer-preview_base")
            .add_task<mo>("modorganizer-diagnose_basic")
            .add_task<mo>("modorganizer-check_fnis")
            .add_task<mo>("modorganizer-installer_bain")
            .add_task<mo>("modorganizer-installer_manual")
            .add_task<mo>("modorganizer-installer_bundle")
            .add_task<mo>("modorganizer-installer_quick")
            .add_task<mo>("modorganizer-installer_fomod")
            .add_task<mo>("modorganizer-installer_fomod_csharp")
            .add_task<mo>("modorganizer-installer_omod")
            .add_task<mo>("modorganizer-installer_wizard")
            .add_task<mo>("modorganizer-bsa_extractor")
            .add_task<mo>("modorganizer-plugin_python");

        add_task<parallel_tasks>()
            .add_task<mo>({"modorganizer-tool_configurator", "pycfg"})
            .add_task<mo>("modorganizer-fnistool")
            .add_task<mo>("modorganizer-basic_games")
            .add_task<mo>({"modorganizer-script_extender_plugin_checker",
                           "scriptextenderpluginchecker"})
            .add_task<mo>({"modorganizer-form43_checker", "form43checker"})
            .add_task<mo>({"modorganizer-preview_dds", "ddspreview"})
            .add_task<mo>({"modorganizer", "organizer"});

        // other tasks

        add_task<translations>();
        add_task<installer>();
    }

    // figures out which command to run and returns it, if any
    //
    std::shared_ptr<command> handle_command_line(const std::vector<std::string>& args)
    {
        auto help  = std::make_shared<help_command>();
        auto build = std::make_shared<build_command>();

        // available commands
        std::vector<std::shared_ptr<command>> commands = {
            help,
            std::make_unique<version_command>(),
            std::make_unique<options_command>(),
            build,
            std::make_unique<pr_command>(),
            std::make_unique<list_command>(),
            std::make_unique<release_command>(),
            std::make_unique<git_command>(),
            std::make_unique<inis_command>(),
            std::make_unique<tx_command>()};

        // commands are shown in the help
        help->set_commands(commands);

        // root group with all the command groups
        clipp::group all_groups;

        // not sure, actually
        all_groups.scoped(false);

        // child groups are exclusive, that is, only one command can be given
        all_groups.exclusive(true);

        for (auto& c : commands)
            all_groups.push_back(c->group());

            // vs reports a no-op on the left side of the command, which is incorrect
#pragma warning(suppress : 4548)
        auto cli = (all_groups, command::common_options_group());
        auto pr  = clipp::parse(args, cli);

        if (!pr) {
            // if a command was picked, show its help instead of the main one
            for (auto&& c : commands) {
                if (c->picked()) {
                    c->force_help();
                    return std::move(c);
                }
            }

            // bad command line
            help->force_exit_code(1);
            return help;
        }

        for (auto&& c : commands) {
            if (c->picked())
                return std::move(c);
        }

        return {};
    }

    int run(const std::vector<std::string>& args)
    {
        font_restorer fr;
        curl_init curl;

        try {
            add_tasks();

            auto c = handle_command_line(args);
            if (!c)
                return 1;

            return c->run();
        }
        catch (bailed&) {
            // silent
            return 1;
        }
    }

}  // namespace mob

int wmain(int argc, wchar_t** argv)
{
    // makes streams unicode
    mob::set_std_streams();

    // outputs stacktrace on crash
    mob::set_thread_exception_handlers();

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.push_back(mob::utf16_to_utf8(argv[i]));

    int r = mob::run(args);
    mob::dump_logs();

    return r;
}

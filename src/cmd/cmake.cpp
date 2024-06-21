#include "pch.h"
#include "../tasks/tasks.h"
#include "commands.h"

namespace mob {

    cmake_command::cmake_command() : command(requires_options) {}

    command::meta_t cmake_command::meta() const
    {
        return {"cmake", "runs cmake in a directory"};
    }

    clipp::group cmake_command::do_group()
    {
        return clipp::group(
            clipp::command("cmake").set(picked_),

            (clipp::option("-h", "--help") >> help_) % "shows this message",

            (clipp::option("-G", "--generator") & clipp::value("GEN") >> gen_) %
                ("sets the -G option for cmake [default: VS]"),

            (clipp::option("-c", "--cmd") & clipp::value("CMD") >> cmd_) %
                "overrides the cmake command line [default: \"..\"]",

            (clipp::option("--x64").set(x64_, true) |
             clipp::option("--x86").set(x64_, false)) %
                "whether to use the x64 or x86 vcvars; if -G is not set, "
                "whether to pass \"-A Win32\" or \"-A x64\" for the default "
                "VS generator [default: x64]",

            (clipp::option("--install-prefix") & clipp::value("PATH") >> prefix_) %
                "sets CMAKE_INSTALL_PREFIX [default: empty]",

            (clipp::option("-d", "--debug").set(debug_, true)) %
                "whether to configure for debug mode [default: false]",

            (clipp::value("PATH") >> path_) % "path from which to run `cmake`");
    }

    int cmake_command::do_run()
    {
        auto t = tasks::modorganizer::create_cmake_tool(
            fs::path(utf8_to_utf16(path_)), mob::cmake::generate,
            debug_ ? config::debug : config::relwithdebinfo);

        t.generator(gen_);
        t.cmd(cmd_);
        t.prefix(prefix_);
        t.output(path_);

        if (!x64_)
            t.architecture(arch::x86);

        // copy the global context, the tool will modify it
        context cxcopy(gcx());

        t.run(cxcopy);

        return 0;
    }

    std::string cmake_command::do_doc()
    {
        return "Runs `cmake ..` in the given directory with the same command line\n"
               "as the one used for modorganizer projects.";
    }

}  // namespace mob

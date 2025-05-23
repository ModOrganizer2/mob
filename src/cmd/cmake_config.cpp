#include "pch.h"

#include "../core/conf.h"
#include "../tasks/tasks.h"

#include "commands.h"

namespace mob {

    cmake_config_command::cmake_config_command() : command(requires_options) {}

    command::meta_t cmake_config_command::meta() const
    {
        return {"cmake-config", "print CMake configuration variables"};
    }

    clipp::group cmake_config_command::do_group()
    {
        return clipp::group(
            clipp::command("cmake-config").set(picked_),
            (clipp::option("-h", "--help") >> help_) % ("shows this message"),
            clipp::command("prefix-path").set(var_, variable::prefix_path) |
                clipp::command("install-prefix").set(var_, variable::install_prefix));
    }

    std::string cmake_config_command::do_doc()
    {
        return "Print CMake variables to be used when configuring projects.\n";
    }

    int cmake_config_command::do_run()
    {
        switch (var_) {
        case variable::prefix_path:
            u8cout << tasks::modorganizer::cmake_prefix_path();
            break;
        case variable::install_prefix:
            u8cout << conf().path().install().string();
            break;
        }
        return 0;
    }
}  // namespace mob

#include "pch.h"
#include "../core/conf.h"
#include "../core/process.h"
#include "tools.h"

namespace mob {

    nmake::nmake() : basic_process_runner("nmake"), arch_(arch::def) {}

    fs::path nmake::binary()
    {
        return conf().tool().get("nmake");
    }

    nmake& nmake::path(const fs::path& p)
    {
        cwd_ = p;
        return *this;
    }

    nmake& nmake::target(const std::string& s)
    {
        target_ = s;
        return *this;
    }

    nmake& nmake::def(const std::string& s)
    {
        def_.push_back(s);
        return *this;
    }

    nmake& nmake::architecture(arch a)
    {
        arch_ = a;
        return *this;
    }

    int nmake::result() const
    {
        return exit_code();
    }

    void nmake::do_run()
    {
        process p;

        p.binary(binary())
            .cwd(cwd_)
            .stderr_filter([](process::filter& f) {
                // initial log line, can't get rid of it, /L or /NOLOGO don't seem
                // to work
                if (f.line.find("Microsoft (R) Macro Assembler (x64)") !=
                    std::string::npos)
                    f.lv = context::level::trace;
                if (f.line.find("Copyright (C) Microsoft Corporation.") !=
                    std::string::npos)
                    f.lv = context::level::trace;
            })
            .arg("/C", process::log_quiet)  // silent
            .arg("/S", process::log_quiet)  // silent
            .arg("/L", process::log_quiet)  // silent, nmake likes to spew crap
            .arg("/D", process::log_dump)   // verbose stuff
            .arg("/P", process::log_dump)   // verbose stuff
            .arg("/W", process::log_dump)   // verbose stuff
            .arg("/K");                     // don't stop on errors

        for (auto&& def : def_)
            p.arg(def);

        p.arg(target_).env(env::vs(arch_));

        execute_and_join(p);
    }

}  // namespace mob

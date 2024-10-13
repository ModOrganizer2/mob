#include "pch.h"
#include "../core/process.h"
#include "tools.h"

namespace mob {

    namespace {
        std::string config_to_string(config c)
        {
            switch (c) {
            case config::debug:
                return "Debug";
            case config::release:
                return "Release";
            case config::relwithdebinfo:
                return "RelWithDebInfo";
            }
            gcx().bail_out(context::generic, "unknow configuration type {}", c);
        }
    }  // namespace

    cmake::cmake(ops o)
        : basic_process_runner("cmake"), op_(o), gen_(vs), arch_(arch::def)
    {
    }

    fs::path cmake::binary()
    {
        return conf().tool().get("cmake");
    }

    cmake& cmake::generator(generators g)
    {
        gen_ = g;
        return *this;
    }

    cmake& cmake::generator(const std::string& g)
    {
        genstring_ = g;
        return *this;
    }

    cmake& cmake::root(const fs::path& p)
    {
        root_ = p;
        return *this;
    }

    cmake& cmake::output(const fs::path& p)
    {
        output_ = p;
        return *this;
    }

    cmake& cmake::prefix(const fs::path& s)
    {
        prefix_ = s;
        return *this;
    }

    cmake& cmake::def(const std::string& name, const std::string& value)
    {
        arg("-D" + name + "=" + value);
        return *this;
    }

    cmake& cmake::def(const std::string& name, const fs::path& p)
    {
        def(name, "\"" + path_to_utf8(p) + "\"");
        return *this;
    }

    cmake& cmake::def(const std::string& name, const char* s)
    {
        def(name, std::string(s));
        return *this;
    }

    cmake& cmake::preset(const std::string& s)
    {
        preset_ = s;
        return *this;
    }

    cmake& cmake::arg(std::string s)
    {
        std::replace(s.begin(), s.end(), '\\', '/');
        args_.push_back(std::move(s));
        return *this;
    }

    cmake& cmake::architecture(arch a)
    {
        arch_ = a;
        return *this;
    }

    cmake& cmake::targets(const std::string& target)
    {
        targets_ = {target};
        return *this;
    }

    cmake& cmake::targets(const std::vector<std::string>& targets)
    {
        targets_ = targets;
        return *this;
    }

    cmake& cmake::configuration(mob::config config)
    {
        config_ = config;
        return *this;
    }

    cmake& cmake::cmd(const std::string& s)
    {
        cmd_ = s;
        return *this;
    }

    fs::path cmake::build_path() const
    {
        // use anything given in output()
        if (!output_.empty())
            return output_;

        // use the build path for the given generator and architecture,
        const auto& g = get_generator(gen_);
        return root_ / (g.output_dir(arch_));
    }

    fs::path cmake::result() const
    {
        return build_path();
    }

    void cmake::do_run()
    {
        switch (op_) {
        case clean: {
            do_clean();
            break;
        }

        case generate: {
            do_generate();
            break;
        }

        case build: {
            do_build();
            break;
        }

        case install: {
            do_install();
            break;
        }

        default: {
            cx().bail_out(context::generic, "bad cmake op {}", op_);
        }
        }
    }

    void cmake::do_generate()
    {
        if (root_.empty())
            cx().bail_out(context::generic, "cmake output path is empty");

        const auto& g = get_generator(gen_);

        auto p = process()
                     .stdout_encoding(encodings::utf8)
                     .stderr_encoding(encodings::utf8)
                     .binary(binary());

        if (!preset_.empty()) {
            p = p.arg("--preset").arg(preset_);
        }

        p = p.arg("-DCMAKE_INSTALL_MESSAGE=" +
                  conf_cmake::to_string(conf().cmake().install_message()))
                .arg("--log-level=ERROR")
                .arg("--no-warn-unused-cli");

        // prefix
        if (!prefix_.empty())
            p.arg("-DCMAKE_INSTALL_PREFIX=", prefix_);

        p.args(args_);

        if (preset_.empty()) {

            if (genstring_.empty()) {
                // there's always a generator name, but some generators don't need
                // an architecture flag, like jom, so get_arch() might return an empty
                // string
                p.arg("-G", "\"" + g.name + "\"")
                    .arg(g.get_arch(arch_))
                    .arg(g.get_host(conf().cmake().host()));
            }
            else {
                // verbatim generator string
                p.arg("-G", "\"" + genstring_ + "\"");
            }

            // `..` by default, overriden by cmd()
            if (cmd_.empty())
                p.arg("..");
            else
                p.arg(cmd_);
        }

        p.env(env::vs(arch_)
                  .set("CXXFLAGS", "/wd4566")
                  .set("VCPKG_ROOT", absolute(conf().path().vcpkg()).string()))
            .cwd(preset_.empty() ? build_path() : root_);

        execute_and_join(p);
    }

    void cmake::do_build()
    {
        auto p = process()
                     .stdout_encoding(encodings::utf8)
                     .stderr_encoding(encodings::utf8)
                     .binary(binary())
                     .arg("--build")
                     .arg(build_path())
                     .arg("--config")
                     .arg(config_to_string(config_));

        for (auto& target : targets_) {
            p = p.arg("--target").arg(target);
        }

        execute_and_join(p);
    }

    void cmake::do_install()
    {
        execute_and_join(process()
                             .stdout_encoding(encodings::utf8)
                             .stderr_encoding(encodings::utf8)
                             .binary(binary())
                             .arg("--install")
                             .arg(build_path())
                             .arg("--config")
                             .arg(config_to_string(config_)));
    }

    void cmake::do_clean()
    {
        cx().trace(context::rebuild, "deleting all generator directories");
        op::delete_directory(cx(), build_path(), op::optional);
    }

    const std::map<cmake::generators, cmake::gen_info>& cmake::all_generators()
    {
        static const std::map<generators, gen_info> map = {
            // jom doesn't need -A for architectures
            {generators::jom, {"build", "NMake Makefiles JOM", "", ""}},

            {generators::vs,
             {"vsbuild", "Visual Studio " + vs::version() + " " + vs::year(), "Win32",
              "x64"}}};

        return map;
    }

    const cmake::gen_info& cmake::get_generator(generators g)
    {
        const auto& map = all_generators();

        auto itor = map.find(g);
        if (itor == map.end())
            gcx().bail_out(context::generic, "unknown generator");

        return itor->second;
    }

    std::string cmake::gen_info::get_arch(arch a) const
    {
        switch (a) {
        case arch::x86: {
            if (x86.empty())
                return {};
            else
                return "-A " + x86;
        }

        case arch::x64: {
            if (x64.empty())
                return {};
            else
                return "-A " + x64;
        }

        case arch::dont_care:
            return {};

        default:
            gcx().bail_out(context::generic, "gen_info::get_arch(): bad arch");
        }
    }

    std::string cmake::gen_info::get_host(std::string_view conf_host) const
    {
        if (conf_host.empty()) {
            return {};
        }

        return "-T host=" + std::string{conf_host};
    }

    std::string cmake::gen_info::output_dir(arch a) const
    {
        switch (a) {
        case arch::x86:
            return (dir + "_32");

        case arch::x64:
        case arch::dont_care:
            return dir;

        default:
            gcx().bail_out(context::generic, "gen_info::get_arch(): bad arch");
        }
    }

}  // namespace mob

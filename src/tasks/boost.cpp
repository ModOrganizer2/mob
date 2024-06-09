#include "pch.h"
#include "../core/process.h"
#include "tasks.h"

namespace mob::tasks {

    namespace {
        std::string boost_version_no_tags()
        {
            const auto v = boost::parsed_version();

            // 1.72[.1]
            std::string s = v.major + "." + v.minor;

            if (v.patch != "")
                s += "." + v.patch;

            return s;
        }

        std::string boost_version_no_tags_underscores()
        {
            return replace_all(boost_version_no_tags(), ".", "_");
        }

        std::string boost_version_all_underscores()
        {
            const auto v = boost::parsed_version();

            // boost_1_72[_0[_b1_rc1]]
            std::string s = "boost_" + v.major + "_" + v.minor;

            if (v.patch != "")
                s += "_" + v.patch;

            if (v.rest != "")
                s += "_" + replace_all(v.rest, "-", "_");

            return s;
        }

        std::string address_model_for_arch(arch a)
        {
            switch (a) {
            case arch::x86:
                return "32";

            case arch::x64:
            case arch::dont_care:
                return "64";

            default:
                gcx().bail_out(context::generic, "boost: bad arch");
            }
        }

        fs::path config_jam_file()
        {
            return boost::source_path() / "user-config-64.jam";
        }

        url prebuilt_url()
        {
            const auto underscores = replace_all(boost::version(), ".", "_");
            return make_prebuilt_url("boost_prebuilt_" + underscores + ".7z");
        }

        url source_url()
        {
            return "https://boostorg.jfrog.io/artifactory/main/release/" +
                   boost_version_no_tags() + "/source/" +
                   boost_version_all_underscores() + ".7z";
        }

        fs::path b2_exe()
        {
            return boost::source_path() / "b2.exe";
        }

    }  // namespace

    boost::boost() : basic_task("boost") {}

    std::string boost::version()
    {
        return conf().version().get("boost");
    }

    std::string boost::version_vs()
    {
        return conf().version().get("boost_vs");
    }

    bool boost::prebuilt()
    {
        return conf().prebuilt().get<bool>("boost");
    }

    fs::path boost::source_path()
    {
        // ex: build/boost_1_74_0
        return conf().path().build() / ("boost_" + boost_version_no_tags_underscores());
    }

    fs::path boost::lib_path(arch a)
    {
        // ex: build/boost_1_74_0/lib64-msvc-14.2/lib
        return root_lib_path(a) / "lib";
    }

    fs::path boost::root_lib_path(arch a)
    {
        // ex: build/boost_1_74_0/lib64-msvc-14.2

        const std::string lib =
            "lib" + address_model_for_arch(a) + "-msvc-" + version_vs();

        return source_path() / lib;
    }

    void boost::do_clean(clean c)
    {
        if (is_set(c, clean::redownload)) {
            // delete downloaded file

            if (prebuilt())
                run_tool(downloader(prebuilt_url(), downloader::clean));
            else
                run_tool(downloader(source_url(), downloader::clean));
        }

        if (is_set(c, clean::reextract)) {
            // delete the whole thing
            cx().trace(context::reextract, "deleting {}", source_path());
            op::delete_directory(cx(), source_path(), op::optional);

            // no need for the rest
            return;
        }

        // those don't make sense for prebults
        if (!prebuilt()) {
            if (is_set(c, clean::reconfigure)) {
                // delete bin and b2.exe to make sure bootstrap runs again
                op::delete_directory(cx(), source_path() / "bin.v2", op::optional);
                op::delete_file(cx(), b2_exe(), op::optional);

                // delete jam files
                op::delete_file(cx(), config_jam_file(), op::optional);
                op::delete_file(cx(), source_path() / "project-config.jam",
                                op::optional);
            }

            if (is_set(c, clean::rebuild)) {
                // delete libs
                op::delete_directory(cx(), root_lib_path(arch::x86), op::optional);
                op::delete_directory(cx(), root_lib_path(arch::x64), op::optional);
            }
        }
    }

    void boost::do_fetch()
    {
        if (prebuilt())
            fetch_prebuilt();
        else
            fetch_from_source();
    }

    void boost::do_build_and_install()
    {
        if (prebuilt())
            build_and_install_prebuilt();
        else
            build_and_install_from_source();
    }

    void boost::fetch_prebuilt()
    {
        cx().trace(context::generic, "using prebuilt boost");

        const auto file = run_tool(downloader(prebuilt_url()));

        run_tool(extractor().file(file).output(source_path()));
    }

    void boost::build_and_install_prebuilt() {}

    void boost::fetch_from_source()
    {
        const auto file = run_tool(downloader(source_url()));

        run_tool(extractor().file(file).output(source_path()));
    }

    void boost::bootstrap()
    {
        // bootstrap b2

        write_config_jam();

        const auto bootstrap = source_path() / "bootstrap.bat";

        run_tool(process_runner(process()
                                    .binary(bootstrap)
                                    .external_error_log(source_path() / "bootstrap.log")
                                    .cwd(source_path())));
    }

    void boost::build_and_install_from_source()
    {
        // bypass boostrap
        if (fs::exists(b2_exe())) {
            cx().trace(context::bypass, "{} exists, boost already bootstrapped",
                       b2_exe());
        }
        else {
            bootstrap();
        }

        // we do not need all variants of all components but since people should
        // usually be using the pre-built, we can build everything without losing too
        // much time and it is much easier to deal when mixing
        //
        // note: filesystem is only required by USVFS I think, so maybe think about
        // removing it if we switch to std::filesystem in USVFS
        //
        clipp::arg_list components{"thread", "date_time", "filesystem", "locale",
                                   "program_options"};

        // static link, static runtime, x64
        do_b2(components, "static", "static", arch::x64);

        // static link, static runtime, x86, required by usvfs 32-bit
        do_b2(components, "static", "static", arch::x86);

        // static link, shared runtime, x64
        do_b2(components, "static", "shared", arch::x64);

        // shared link, shared runtime, x64
        do_b2(components, "shared", "shared", arch::x64);
    }

    void boost::do_b2(const std::vector<std::string>& components,
                      const std::string& link, const std::string& runtime_link, arch a)
    {
        // will transform all components to --with-X

        run_tool(process_runner(process()
                                    .binary(b2_exe())
                                    .arg("address-model=", address_model_for_arch(a))
                                    .arg("link=", link)
                                    .arg("runtime-link=", runtime_link)
                                    .arg("toolset=", "msvc-" + vs::toolset())
                                    .arg("--user-config=", config_jam_file())
                                    .arg("--stagedir=", root_lib_path(a))
                                    .arg("--libdir=", root_lib_path(a))
                                    .args(map(components,
                                              [](auto&& c) {
                                                  return "--with-" + c;
                                              }))
                                    .env(env::vs(a))
                                    .cwd(source_path())));
    }

    void boost::write_config_jam()
    {
        // b2 requires forward slashes
        auto forward_slashes = [](auto&& p) {
            std::string s = path_to_utf8(p);
            return replace_all(s, "\\", "/");
        };

        // this currently writes an empty configuration file, at some point it was used
        // to configure the Boost.Python built
        //
        // kept here in case we need a custom user-configuration in the future
        //
        std::ostringstream oss;

        // logging
        {
            cx().trace(context::generic,
                       "writing config file at {}:", config_jam_file());

            for_each_line(oss.str(), [&](auto&& line) {
                cx().trace(context::generic, "        {}", line);
            });
        }

        // writing
        op::write_text_file(cx(), encodings::utf8, config_jam_file(), oss.str());
    }

    boost::version_info boost::parsed_version()
    {
        // 1.72.0-b1-rc1
        // everything but 1.72 is optional
        std::regex re("(\\d+)\\."  // 1.
                      "(\\d+)"     // 72
                      "(?:"
                      "\\.(\\d+)"  // .0
                      "(?:"        //
                      "-(.+)"      // -b1-rc1
                      ")?"
                      ")?");

        std::smatch m;

        const auto s = version();

        if (!std::regex_match(s, m, re))
            gcx().bail_out(context::generic, "bad boost version '{}'", s);

        return {m[1], m[2], m[3], m[4]};
    }

}  // namespace mob::tasks

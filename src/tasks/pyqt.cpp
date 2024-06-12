#include "pch.h"
#include "../core/process.h"
#include "tasks.h"

// build process for python, sip and pyqt; if one is built from source, all
// three need to be built from source, plus openssl because python needs it
//
//  1) build openssl
//
//  2) build python, needs openssl
//
//  3) build sip, needs python:
//      - download and extract source archive
//      - run `python setup.py install` in sip's directory, this generates
//        sip-install.exe and sip-module.exe in python-XX/Scripts (among others)
//      - run `sip-module.exe --sip-h` to generate a sip.h file in sip's source
//        directory
//      - that header file is copied into python/include and is included by
//        by plugin_python in sipapiaccess.h
//
// 4) build pyqt, needs sip
//      - download and extract source archive
//      - use `pip install` to install PyQt-builder
//      - run `sip-install.exe` with the list of required modules, creating a
//        folder for each module in PyQt6-XX/build/ with `.pyd` files
//      - run `sip-module.exe --sdist`, which creates
//        downloads/PyQt6_sip-XXX.tar.gz
//      - run `pip install` with that file, which creates
//        `python-XX/Lib/site-packages/PyQt6/sip.cp32-win_amd64.pyd`
//      - for installation, a bunch of files from site-packages/PyQt6/ are
//        copied into install/bin/plugins/data/PyQt6, including a .pyi file from
//        sip

namespace mob::tasks {

    namespace {

        url source_url()
        {
            const auto version = pyqt::version();
            std::string base   = "https://pypi.io/packages/source/P/PyQt6/";
            if (version.find("dev") != std::string::npos) {
                base = "https://riverbankcomputing.com/pypi/packages/PyQt6/";
            }
            return base + "PyQt6-" + pyqt::version() + ".tar.gz";
        }

        url prebuilt_url()
        {
            return make_prebuilt_url("PyQt6_gpl-prebuilt-" + pyqt::version() + ".7z");
        }

        // file created by sip-module.exe
        //
        fs::path sip_install_file()
        {
            return "PyQt6_sip-" + sip::version_for_pyqt() + ".tar.gz";
        }

    }  // namespace

    pyqt::pyqt() : basic_task("pyqt") {}

    std::string pyqt::version()
    {
        return conf().version().get("pyqt");
    }

    std::string pyqt::builder_version()
    {
        return conf().version().get("pyqt_builder");
    }

    bool pyqt::prebuilt()
    {
        return conf().prebuilt().get<bool>("pyqt");
    }

    fs::path pyqt::source_path()
    {
        return conf().path().build() / ("PyQt6-" + version());
    }

    fs::path pyqt::build_path()
    {
        return source_path() / "build";
    }

    config pyqt::build_type()
    {
        return conf().build_types().get("pyqt");
    }

    std::string pyqt::pyqt_sip_module_name()
    {
        return "PyQt6.sip";
    }

    void pyqt::do_clean(clean c)
    {
        if (prebuilt()) {
            // delete prebuilt download
            if (is_set(c, clean::redownload))
                run_tool(downloader(prebuilt_url(), downloader::clean));
        }
        else {
            // delete source download
            if (is_set(c, clean::redownload))
                run_tool(downloader(source_url(), downloader::clean));
        }

        // delete whole directory
        if (is_set(c, clean::reextract)) {
            cx().trace(context::reextract, "deleting {}", source_path());
            op::delete_directory(cx(), source_path(), op::optional);

            // no need to do anything else
            return;
        }

        if (!prebuilt()) {
            // delete the pyqt-sip file that's created when building from source
            if (is_set(c, clean::rebuild)) {
                op::delete_file(cx(), conf().path().cache() / sip_install_file(),
                                op::optional);
            }
        }
    }

    void pyqt::do_fetch()
    {
        if (prebuilt())
            fetch_prebuilt();
        else
            fetch_from_source();
    }

    void pyqt::do_build_and_install()
    {
        if (prebuilt())
            build_and_install_prebuilt();
        else
            build_and_install_from_source();
    }

    void pyqt::fetch_prebuilt()
    {
        const auto file = run_tool(downloader(prebuilt_url()));

        run_tool(extractor().file(file).output(source_path()));
    }

    void pyqt::build_and_install_prebuilt()
    {
        // copy the prebuilt files directly into the python directory, they're
        // required by sip, which is always built from source
        op::copy_glob_to_dir_if_better(cx(), source_path() / "*", python::source_path(),
                                       op::copy_files | op::copy_dirs);

        // copy files to build/install for MO
        copy_files();
    }

    void pyqt::fetch_from_source()
    {
        const auto file = run_tool(downloader(source_url()));

        run_tool(extractor().file(file).output(source_path()));
    }

    void pyqt::build_and_install_from_source()
    {
        // use pip to install the pyqt builder
        if (python::build_type() == config::debug) {
            // PyQt-builder has sip as a dependency, so installing it directly will
            // replace the sip we have installed manually, but the installed sip will
            // not work (see comment in sip::build() for details)
            //
            // the workaround is to install the dependencies manually (only packaging),
            // and then use a --no-dependencies install with pip
            //
            run_tool(pip(pip::install).package("packaging"));
            run_tool(pip(pip::install)
                         .package("PyQt-builder")
                         .no_dependencies()
                         .version(builder_version()));
        }
        else {
            run_tool(
                pip(pip::install).package("PyQt-builder").version(builder_version()));
        }

        // patch for builder.py
        run_tool(patcher()
                     .task(name())
                     .file("builder.py.manual_patch")
                     .root(python::site_packages_path() / "pyqtbuild"));

        // build modules and generate the PyQt6_sip-XX.tar.gz file
        sip_build();

        // run pip install for the PyQt6_sip-XX.tar.gz file
        install_sip_file();

        // copy files to build/install for MO
        copy_files();
    }

    void pyqt::sip_build()
    {
        // put qt and python in the path, set CL and LIB, which are used by the
        // visual c++ compiler that's eventually spawned, and set PYTHONHOME
        auto pyqt_env =
            env::vs_x64()
                .append_path({qt::bin_path(), python::build_path(),
                              python::source_path(), python::scripts_path()})
                .set("CL", " /MP")
                .set("LIB", ";" + path_to_utf8(conf().path().install_libs()),
                     env::append)
                .set("PYTHONHOME", path_to_utf8(python::source_path()));

        // create a bypass file, because pyqt always tries to build stuff and it
        // takes forever
        bypass_file built_bypass(cx(), source_path(), "built");

        if (built_bypass.exists()) {
            cx().trace(context::bypass, "pyqt already built");
        }
        else {
            // sip-install.exe has trouble with deleting the build/ directory and
            // trying to recreate it too fast, giving an access denied error; do it
            // here instead
            op::delete_directory(cx(), source_path() / "build", op::optional);

            auto p = sip::sip_install_process()
                         .arg("--confirm-license")
                         .arg("--verbose", process::log_trace)
                         .arg("--pep484-pyi")
                         .arg("--link-full-dll")
                         .arg("--build-dir", build_path())
                         .cwd(source_path())
                         .env(pyqt_env);

            if (build_type() == config::debug) {
                p.arg("--debug");
            }

            // build modules
            run_tool(process_runner(p));

            // done, create the bypass file
            built_bypass.create();
        }

        // generate the PyQt6_sip-XX.tar.gz file
        run_tool(process_runner(sip::sip_module_process()
                                    .arg("--sdist")
                                    .arg(pyqt_sip_module_name())
                                    .cwd(conf().path().cache())
                                    .env(pyqt_env)));
    }

    void pyqt::install_sip_file()
    {
        // create a bypass file, because pyqt always tries to install stuff and it
        // takes forever
        bypass_file installed_bypass(cx(), source_path(), "installed");

        if (installed_bypass.exists()) {
            cx().trace(context::bypass, "pyqt already installed");
        }
        else {
            // run `pip install` on the generated PyQt6_sip-XX.tar.gz file
            run_tool(
                pip(pip::install).file(conf().path().cache() / sip_install_file()));

            // done, create the bypass file
            installed_bypass.create();
        }
    }

    void pyqt::copy_files()
    {
        // pyqt puts its files in python-XX/Lib/site-packages/PyQt6
        const fs::path site_packages_pyqt = python::site_packages_path() / "PyQt6";

        // copying some dlls from Qt's installation directory into
        // python-XX/PCBuild/amd64, those are needed by PyQt6 when building several
        // projects

        const std::vector<std::string> dlls{"Qt6Core", "Qt6Xml"};

        for (auto dll : dlls) {
            if (build_type() == config::debug) {
                dll += "d";
            }
            dll += ".dll";
            op::copy_file_to_dir_if_better(
                cx(), qt::bin_path() / dll, python::build_path(),
                op::unsafe);  // source file is outside prefix
        }

        // installation of PyQt6 python files (.pyd, etc.) is done
        // by the python plugin directly
    }

}  // namespace mob::tasks

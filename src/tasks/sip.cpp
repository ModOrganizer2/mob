#include "pch.h"
#include "../core/process.h"
#include "tasks.h"

// see the top of pyqt.cpp for some stuff about python/sip/pyqt

namespace mob::tasks {

    namespace {

        fs::path download_file()
        {
            return conf().path().cache() / ("sip-" + sip::version() + ".tar.gz");
        }

        std::string version_for_module_source()
        {
            // 12.7.2
            // .2 is optional
            std::regex re(R"((\d+)\.(\d+)(?:\.(\d+))?)");
            std::smatch m;

            const auto s = sip::version_for_pyqt();

            if (!std::regex_match(s, m, re))
                gcx().bail_out(context::generic, "bad pyqt sip version {}", s);

            // 12.7
            return m[1].str();  // + "." + m[2].str();
        }

        // header file generated by `sip-module.exe` at the end of the build process,
        // used as a bypass file and also copied into python's include directory,
        // plugin_python needs it
        //
        fs::path sip_header_file()
        {
            return sip::source_path() / "sip.h";
        }

    }  // namespace

    sip::sip() : basic_task("sip") {}

    std::string sip::version()
    {
        return conf().version().get("sip");
    }

    std::string sip::version_for_pyqt()
    {
        return conf().version().get("pyqt_sip");
    }

    bool sip::prebuilt()
    {
        return false;
    }

    fs::path sip::source_path()
    {
        return conf().path().build() / ("sip-" + version());
    }

    process sip::sip_module_process()
    {
        return process().binary(python::scripts_path() / "sip-module.exe");
    }

    process sip::sip_install_process()
    {
        return process().binary(python::scripts_path() / "sip-install.exe");
    }

    fs::path sip::module_source_path()
    {
        // 12.7
        const auto dir = version_for_module_source();

        return source_path() / "sipbuild" / "module" / "source" / dir;
    }

    void sip::do_clean(clean c)
    {
        // delete file downloaded by pip
        if (is_set(c, clean::redownload)) {
            if (fs::exists(download_file())) {
                cx().trace(context::redownload, "deleting {}", download_file());
                op::delete_file(cx(), download_file(), op::optional);
            }
        }

        // delete the whole thing
        if (is_set(c, clean::reextract)) {
            cx().trace(context::reextract, "deleting {}", source_path());
            op::delete_directory(cx(), source_path(), op::optional);
        }

        // delete the whole build directory
        if (is_set(c, clean::rebuild)) {
            op::delete_directory(cx(), source_path() / "build", op::optional);

            if (fs::exists(sip_header_file()))
                op::delete_file(cx(), sip_header_file(), op::optional);
        }

        // note that there's a bunch of files still left python-XX/Scripts that
        // can't be easily deleted except by deleting something like "sip-*", but
        // there might be other stuff in there
    }

    void sip::do_fetch()
    {
        if (fs::exists(download_file())) {
            cx().trace(context::bypass, "sip: {} already exists", download_file());
        }
        else {
            // download
            run_tool(pip(pip::download).package("sip").version(version()));
        }

        // extract
        run_tool(extractor().file(download_file()).output(source_path()));
    }

    void sip::do_build_and_install()
    {
        if (fs::exists(sip_header_file())) {
            cx().trace(context::bypass, "{} already exists", sip_header_file());
        }
        else {
            build();
            generate_header();
        }

        // sip.h is included by sipapiaccess.h in plugin_python and it assumes it's
        // in the include path
        op::copy_file_to_dir_if_better(cx(), sip_header_file(), python::include_path());
    }

    void sip::build()
    {
        if (python::build_type() == config::debug) {
            // if Python is build in debug mode, fall back to old setup.py because pip
            // install seems to generated broken script wrapper that point to a
            // non-existing python.exe instead of python_d.exe
            run_tool(pip(pip::install).package("setuptools"));
            run_tool(mob::python().root(source_path()).arg("setup.py").arg("install"));
        }
        else {
            run_tool(pip(pip::install).file(source_path()));
        }
    }

    void sip::convert_script_file_to_acp(const std::string& filename)
    {
        // all the various .py files that were installed in python-XX/Scripts/
        // by build() above have a shebang that has the absolute path to the python
        // executable
        //
        // these .py files are used by their corresponding .exe file, like
        // `sip-module.exe` calls into `sip-module.py`
        //
        // the files are encoded in utf-8, but the .exe's will fail to execute them
        // if they have non-ascii characters in the python path
        //
        // this converts the files into ACP, hoping that the characters actually
        // exist in the codepage, so that the .exe can run
        //
        // if the path contains utf-8 characters that don't exist in the ACP,
        // conversion will fail and U+FFFD will be written, but it would have failed
        // anyway

        const fs::path src    = python::scripts_path() / filename;
        const fs::path backup = python::scripts_path() / (filename + ".bak");
        const fs::path dest   = python::scripts_path() / (filename + ".acp");

        if (!fs::exists(backup)) {
            cx().debug(context::generic, "converting {} to acp", src);

            // read the utf8 file
            const std::string utf8 = op::read_text_file(cx(), encodings::utf8, src);

            // convert to acp and write to filename.acp
            op::write_text_file(cx(), encodings::acp, dest, utf8);

            // rename the source to .bak and rename filename.acp to the original
            op::replace_file(cx(), src, dest, backup);
        }
    }

    void sip::generate_header()
    {
        // generate sip.h, will be copied to python's include directory, used
        // by plugin_python
        run_tool(process_runner(sip_module_process()
                                    .chcp(65001)
                                    .stdout_encoding(encodings::acp)
                                    .stderr_encoding(encodings::acp)
                                    .arg("--sip-h")
                                    .arg(pyqt::pyqt_sip_module_name())
                                    .cwd(source_path())));
    }

}  // namespace mob::tasks

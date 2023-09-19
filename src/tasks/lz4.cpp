#include "pch.h"
#include "tasks.h"

namespace mob::tasks {

    namespace {

        fs::path solution_dir()
        {
            return lz4::source_path() / "build" / "VS2022";
        }

        fs::path solution_file()
        {
            return solution_dir() / "lz4.sln";
        }

        fs::path out_dir()
        {
            return solution_dir() / "bin" / "x64_Release";
        }

        url prebuilt_url()
        {
            return make_prebuilt_url("lz4_prebuilt_" + lz4::version() + ".7z");
        }

    }  // namespace

    lz4::lz4() : basic_task("lz4") {}

    std::string lz4::version()
    {
        return conf().version().get("lz4");
    }

    bool lz4::prebuilt()
    {
        return conf().prebuilt().get<bool>("lz4");
    }

    fs::path lz4::source_path()
    {
        return conf().path().build() / ("lz4-" + version());
    }

    void lz4::do_clean(clean c)
    {
        if (prebuilt()) {
            // delete download
            if (is_set(c, clean::redownload))
                run_tool(downloader(prebuilt_url(), downloader::clean));

            // delete the whole directory
            if (is_set(c, clean::reextract)) {
                cx().trace(context::reextract, "deleting {}", source_path());
                op::delete_directory(cx(), source_path(), op::optional);
            }
        }
        else {
            // delete the whole directory
            if (is_set(c, clean::reclone)) {
                git_wrap::delete_directory(cx(), source_path());

                // no point in doing anything more
                return;
            }

            // msbuild clean
            if (is_set(c, clean::rebuild))
                run_tool(create_msbuild_tool(msbuild::clean));
        }
    }

    void lz4::do_fetch()
    {
        if (prebuilt())
            fetch_prebuilt();
        else
            fetch_from_source();
    }

    void lz4::do_build_and_install()
    {
        if (prebuilt())
            build_and_install_prebuilt();
        else
            build_and_install_from_source();
    }

    void lz4::fetch_prebuilt()
    {
        cx().trace(context::generic, "using prebuilt lz4");

        const auto file = run_tool(downloader(prebuilt_url()));

        run_tool(extractor().file(file).output(source_path()));
    }

    void lz4::build_and_install_prebuilt()
    {
        // copy pdb and dll

        op::copy_file_to_dir_if_better(cx(), source_path() / "bin" / "liblz4.pdb",
                                       conf().path().install_pdbs());

        op::copy_file_to_dir_if_better(cx(), source_path() / "bin" / "liblz4.dll",
                                       conf().path().install_dlls());
    }

    void lz4::fetch_from_source()
    {
        // clone
        run_tool(make_git()
                     .url(make_git_url("lz4", "lz4"))
                     .branch(version())
                     .root(source_path()));
    }

    void lz4::build_and_install_from_source()
    {
        run_tool(create_msbuild_tool());

        // cmake_common looks for the lib files in the bin/ directory, which is
        // correct for prebuilts, but not from source, so copy the files in there
        op::copy_glob_to_dir_if_better(cx(), out_dir() / "*", source_path() / "bin",
                                       op::copy_files);

        // copy dll and pdb
        op::copy_file_to_dir_if_better(cx(), source_path() / "bin" / "liblz4.dll",
                                       conf().path().install_dlls());

        op::copy_file_to_dir_if_better(cx(), source_path() / "bin" / "liblz4.pdb",
                                       conf().path().install_pdbs());
    }

    msbuild lz4::create_msbuild_tool(msbuild::ops o)
    {
        return std::move(msbuild(o).solution(solution_file()).targets({"liblz4-dll"}));
    }

}  // namespace mob::tasks

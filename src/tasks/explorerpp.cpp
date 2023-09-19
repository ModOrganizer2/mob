#include "pch.h"
#include "tasks.h"

namespace mob::tasks {

    namespace {

        url source_url()
        {
            return "https://explorerplusplus.com/software/"
                   "explorer++_" +
                   explorerpp::version() + "_x64.zip";
        }

    }  // namespace

    explorerpp::explorerpp() : basic_task("explorerpp", "explorer++") {}

    std::string explorerpp::version()
    {
        return conf().version().get("explorerpp");
    }

    bool explorerpp::prebuilt()
    {
        // always prebuilt, direct download
        return false;
    }

    fs::path explorerpp::source_path()
    {
        return conf().path().build() / "explorer++";
    }

    void explorerpp::do_clean(clean c)
    {
        // delete download
        if (is_set(c, clean::redownload))
            run_tool(downloader(source_url(), downloader::clean));

        // delete the whole directory
        if (is_set(c, clean::reextract)) {
            cx().trace(context::reextract, "deleting {}", source_path());
            op::delete_directory(cx(), source_path(), op::optional);
        }
    }

    void explorerpp::do_fetch()
    {
        const auto file = run_tool(downloader(source_url()));

        run_tool(extractor().file(file).output(source_path()));

        // copy everything to install/bin/explorer++
        op::copy_glob_to_dir_if_better(cx(), source_path() / "*",
                                       conf().path().install_bin() / "explorer++",
                                       op::copy_files);
    }

}  // namespace mob::tasks

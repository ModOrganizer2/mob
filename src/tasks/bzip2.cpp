#include "pch.h"
#include "tasks.h"

namespace mob::tasks {

    // required by python

    namespace {

        url source_url()
        {
            return "https://sourceware.org/pub/bzip2/"
                   "bzip2-" +
                   bzip2::version() + ".tar.gz";
        }

    }  // namespace

    bzip2::bzip2() : basic_task("bzip2") {}

    std::string bzip2::version()
    {
        return conf().version().get("bzip2");
    }

    bool bzip2::prebuilt()
    {
        // no prebuilts, just the source, required by python, which uses the
        // source files directly
        return false;
    }

    fs::path bzip2::source_path()
    {
        return conf().path().build() / ("bzip2-" + version());
    }

    void bzip2::do_clean(clean c)
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

    void bzip2::do_fetch()
    {
        const auto file = run_tool(downloader(source_url()));

        run_tool(extractor().file(file).output(source_path()));
    }

}  // namespace mob::tasks

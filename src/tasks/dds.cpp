#include "pch.h"
#include "tasks.h"

namespace mob::tasks {

    dds::dds() : basic_task("dds") {}

    bool dds::prebuilt()
    {
        return false;
    }

    fs::path dds::source_path()
    {
        return conf().path().build() / ("dds-header");
    }

    void dds::do_clean(clean c)
    {
        // delete the whole directory
        if (is_set(c, clean::reclone)) {
            git_wrap::delete_directory(cx(), source_path());

            // no point in doing anything more
            return;
        }
    }

    void dds::do_fetch()
    {
        auto file = run_tool(downloader("https://raw.githubusercontent.com/Microsoft/"
                                        "DirectXTex/main/DirectXTex/DDS.h"));
        if (!exists(source_path()))
            op::create_directories(gcx(), source_path());
        op::copy_file_to_dir_if_better(cx(), file, source_path());
    }
}  // namespace mob::tasks

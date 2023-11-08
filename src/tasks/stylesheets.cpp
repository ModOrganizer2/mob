#include "pch.h"
#include "tasks.h"

namespace mob::tasks {

    std::vector<stylesheets::release> releases()
    {
        return {{"6788-00", "paper-light-and-dark",
                 conf().version().get("ss_paper_lad_6788"), "paper-light-and-dark", ""},

                {"6788-00", "paper-automata",
                 conf().version().get("ss_paper_automata_6788"), "3.0.Paper.Automata",
                 "2. Paper Automata-64439-A2-3-0-1610629680"},

                {"6788-00", "paper-mono", conf().version().get("ss_paper_mono_6788"),
                 "Paper-Mono", ""},

                {"6788-00", "1809-dark-mode",
                 conf().version().get("ss_dark_mode_1809_6788"), "1809", ""},

                {"Trosski", "ModOrganizer_Style_Morrowind",
                 conf().version().get("ss_morrowind_trosski"),
                 "Morrowind-MO2-Stylesheet", ""},

                {"Trosski", "Mod-Organizer-2-Skyrim-Stylesheet",
                 conf().version().get("ss_skyrim_trosski"), "Skyrim-MO2-Stylesheet",
                 ""},

                {"Trosski", "ModOrganizer_Style_Fallout3",
                 conf().version().get("ss_fallout3_trosski"), "Fallout3-MO2-Stylesheet",
                 ""},

                {"Trosski", "Mod-Organizer2-Fallout-4-Stylesheet",
                 conf().version().get("ss_fallout4_trosski"), "Fallout4-MO2-Stylesheet",
                 ""},

                {"Trosski", "Starfield_MO2_Stylesheet",
                 conf().version().get("ss_starfield_trosski"), "Transparent-Starfield_Stylesheet",
                 ""}};
    }

    stylesheets::stylesheets() : task("ss", "stylesheets") {}

    bool stylesheets::prebuilt()
    {
        return false;
    }

    void stylesheets::do_clean(clean c)
    {
        // delete download file for each release
        if (is_set(c, clean::redownload)) {
            for (auto&& r : releases())
                run_tool(make_downloader_tool(r, downloader::clean));
        }

        // delete directory for each release
        if (is_set(c, clean::reextract)) {
            for (auto&& r : releases()) {
                const auto p = release_build_path(r);

                cx().trace(context::reextract, "deleting {}", p);
                op::delete_directory(cx(), p, op::optional);
            }
        }
    }

    void stylesheets::do_fetch()
    {
        // download and extract file for each release
        for (auto&& r : releases()) {
            const auto file = run_tool(make_downloader_tool(r));

            run_tool(extractor().file(file).output(release_build_path(r)));
        }
    }

    fs::path stylesheets::release_build_path(const release& r) const
    {
        // something like build/paper-mono-v2.1
        return conf().path().build() / (r.repo + "-" + r.version);
    }

    downloader stylesheets::make_downloader_tool(const release& r,
                                                 downloader::ops o) const
    {
        url u = "https://github.com/" + r.user + "/" + r.repo +
                "/releases/"
                "download/" +
                r.version + "/" + r.file + ".7z";

        return std::move(
            downloader(o).url(u).file(conf().path().cache() / (r.repo + ".7z")));
    }

    void stylesheets::do_build_and_install()
    {
        for (auto&& r : releases()) {
            // copy all the files and directories from the source directory directly
            // into install/bin/stylesheets
            op::copy_glob_to_dir_if_better(
                cx(),
                r.top_level_folder.size()
                    ? release_build_path(r) / r.top_level_folder / "*"
                    : release_build_path(r) / "*",
                conf().path().install_stylesheets(), op::copy_files | op::copy_dirs);
        }
    }

}  // namespace mob::tasks

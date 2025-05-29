#include "pch.h"
#include "../core/env.h"
#include "../utility/threading.h"
#include "task_manager.h"
#include "tasks.h"

// the translations task creates a directory build/transifex-translations
// managed by the transifex tool, it has a .tx directory with a config file in
// there
//
// checking out the translations from transifex will create a second directory
// "translations", which will contain one directory per project on transifex
//
// for example:
//
//  build/
//   +- transifex-translations/
//       +- .tx/
//       +- translations/
//           +- mod-organizer-2.bsa_extractor
//           +- mod-organizer-2.bsa_packer
//           +- mod-organizer-2.check_fnis
//           ...
//
// each project directory has a bunch of .ts files, one per language, such as:
//
//   - de.ts
//   - es.ts
//   - fi.ts
//   - ...
//
// .ts files are text files with translations, they are compiled with `lrelease`
// (a qt tool) to create a .qm file, which can be loaded by mo at runtime
//
// normally, one .ts creates one .qm file, but some projects need more than one
// .ts, such as all the gamebryo projects
//
// there are a bunch of strings in the gamebryo project itself that are shared
// with all the actual gamebryo game plugins; historically, these strings were
// duplicated to the .ts file of every plugin
//
// nowadays, the strings are not duplicated anymore and only exist in the .ts
// file for gamebryo, which means that compiling the translations for a gamebryo
// plugin needs _both_ the .ts file of the plugin and the .ts file of gamebryo
//
// when walking the project directories in translations, each project name is
// searched in the list of mob tasks (they must exist) and tasks have a flag for
// whether they're a gamebryo project or not; if the task is a gamebryo project,
// the .ts for gamebryo is also added to the list of files that need to be
// compiled
//
// this can create some discrepancies if a translation exists for a plugin, but
// not for gamebryo; for example, someone translating the skyrim plugin to
// ancient Greek, but not doing so for gamebryo; in that case, a warning is
// logged (see handle_ts_file())
//
//
// so, constructing a translations::projects object by giving it the path to the
// translations/ directory will walk the tree and build the list of projects and
// files that need to be compiled
//
// compiled .qm files will be put in install/bin/translations

namespace mob::tasks {

    translations::projects::lang::lang(std::string n) : name(std::move(n)) {}

    std::pair<std::string, std::string> translations::projects::lang::split() const
    {
        const auto p = name.find('_');

        if (p == std::string::npos)
            return {{}, name};

        return {name.substr(0, p), name.substr(p + 1)};
    }

    translations::projects::project::project(std::string n) : name(std::move(n)) {}

    translations::projects::projects(fs::path root) : root_(std::move(root))
    {
        try {
            // walk all directories in the root, each one is a project directory that
            // contains .ts files
            for (auto e : fs::directory_iterator(root_)) {
                if (!e.is_directory())
                    continue;

                auto p = create_project(e.path());
                if (!p.name.empty())
                    projects_.push_back(p);
            }
        }
        catch (std::exception& e) {
            gcx().bail_out(context::generic, "can't walk {} for projects, {}", root_,
                           e.what());
        }
    }

    const std::vector<translations::projects::project>&
    translations::projects::get() const
    {
        return projects_;
    }

    const std::vector<std::string>& translations::projects::warnings() const
    {
        return warnings_;
    }

    std::optional<translations::projects::project>
    translations::projects::find(std::string_view name) const
    {
        for (auto&& p : projects_) {
            if (p.name == name)
                return p;
        }

        return {};
    }

    translations::projects::project
    translations::projects::create_project(const fs::path& dir)
    {
        // walks all the .ts files in the project, creates a `lang` object for
        // each
        //

        // splitting
        const auto dir_name = path_to_utf8(dir.filename());
        const auto dir_cs   = split(dir_name, ".");

        if (dir_cs.size() != 2) {
            warnings_.push_back(
                ::std::format("bad directory name '{}'; skipping", dir_name));

            return {};
        }

        const auto project_name = trim_copy(dir_cs[1]);
        if (project_name.empty()) {
            warnings_.push_back(
                ::std::format("bad directory name '{}', skipping", dir_name));

            return {};
        }

        // project
        project p(project_name);

        // for each file
        for (auto f : fs::directory_iterator(dir)) {
            if (!f.is_regular_file())
                continue;

            const auto path = f.path();

            // there should only be .ts files in there
            if (path.extension() != ".ts") {
                warnings_.push_back(
                    ::std::format("{} is not a .ts file", path_to_utf8(path)));

                continue;
            }

            // add a new `lang` object for it
            p.langs.push_back(create_lang(project_name, f.path()));
        }

        return p;
    }

    translations::projects::lang
    translations::projects::create_lang(const std::string& project_name,
                                        const fs::path& main_ts_file)
    {
        lang lg(path_to_utf8(main_ts_file.stem()));

        // every lang has the .ts file from the project, gamebryo plugins have more
        lg.ts_files.push_back(main_ts_file);

        return lg;
    }

    translations::translations() : task("translations") {}

    fs::path translations::source_path()
    {
        return conf().path().build() / "transifex-translations";
    }

    void translations::do_clean(clean c)
    {
        // delete the whole directory
        if (is_set(c, clean::redownload))
            op::delete_directory(cx(), source_path(), op::optional);

        // remove the .qm files in the translations/ directory
        if (is_set(c, clean::rebuild)) {
            op::delete_file_glob(cx(), conf().path().install_translations() / "*.qm",
                                 op::optional);
        }
    }

    void translations::do_fetch()
    {
        // 1) initialize the directory with the transifex tool to create the .tx
        //    directory
        // 2) configure the tx directory so it knows the url
        // 3) pull translations from transifex

        // api key
        const std::string key = conf().transifex().get("key");

        if (key.empty() && !this_env::get_opt("TX_TOKEN")) {
            cx().warning(context::generic,
                         "no key was in the INI and the TX_TOKEN env variable doesn't "
                         "exist, this will probably fail");
        }

        // transifex url
        const url u = conf().transifex().get("url") + "/" +
                      conf().transifex().get("team") + "/" +
                      conf().transifex().get("project") + "/dashboard";

        // initializing
        cx().debug(context::generic, "init tx");
        run_tool(transifex(transifex::init).root(source_path()));

        // configuring
        if (conf().transifex().get<bool>("configure")) {
            cx().debug(context::generic, "configuring");
            run_tool(
                transifex(transifex::config).root(source_path()).api_key(key).url(u));
        }
        else {
            cx().trace(context::generic, "skipping configuring");
        }

        // pulling
        if (conf().transifex().get<bool>("pull")) {
            cx().debug(context::generic, "pulling");
            run_tool(transifex(transifex::pull)
                         .root(source_path())
                         .api_key(key)
                         .minimum(conf().transifex().get<int>("minimum"))
                         .force(conf().transifex().get<bool>("force")));
        }
        else {
            cx().trace(context::generic, "skipping pulling");
        }
    }

    void translations::do_build_and_install()
    {
        // 1) build the list of projects, languages and .ts files
        // 2) run `lrelease` for every language in every project
        // 3) copy builtin qt translations

        const auto root = source_path() / "translations";
        const auto dest = conf().path().install_translations();
        const projects ps(root);

        op::create_directories(cx(), dest);

        // log all the warnings added while walking the projects
        for (auto&& w : ps.warnings())
            cx().warning(context::generic, "{}", w);

        // run `lrelease` in a thread pool
        parallel_functions v;

        // for each project
        for (auto& p : ps.get()) {
            // for each language
            for (auto& lg : p.langs) {
                // add a functor that will run lrelease
                v.push_back(
                    {lg.name + "." + p.name, [&] {
                         // run release for the given project name and list of .ts files
                         run_tool(
                             lrelease().project(p.name).sources(lg.ts_files).out(dest));
                     }});
            }
        }

        // run all the functors in parallel
        parallel(v);

        if (auto p = ps.find("organizer"))
            copy_builtin_qt_translations(*p, dest);
        else
            cx().bail_out(context::generic, "organizer project not found");
    }

    void translations::copy_builtin_qt_translations(const projects::project& p,
                                                    const fs::path& dest)
    {
        // list of prefixes in the qt translations directory
        const std::vector<std::string> prefixes = {"qt", "qtbase"};

        // tries to copy the .qm file, returns false if the file doesn't exist
        auto try_copy = [&](auto&& prefix, auto&& lang) {
            const std::string file = prefix + "_" + lang + ".qm";
            const fs::path src     = conf().path().qt_translations() / file;

            if (!fs::exists(src))
                return false;

            op::copy_file_to_dir_if_better(cx(), src, dest, op::unsafe);
            return true;
        };

        for (auto&& prefix : prefixes) {
            cx().debug(context::generic, "copying builtin qt translations '{}'",
                       prefix);

            for (auto&& lg : p.langs) {
                // some source files use 'country_lang', others are just 'lang',
                // such as "qt_pl.qm" and "qt_zh_CN.qm", so try both

                if (try_copy(prefix, lg.name))
                    continue;

                const auto [language, country] = lg.split();

                if (!country.empty()) {
                    if (try_copy(prefix, language))
                        continue;
                }

                cx().warning(context::generic,
                             "missing builtin qt translation '{}' for lang {} from {}",
                             prefix, lg.name, conf().path().qt_translations());
            }
        }
    }

}  // namespace mob::tasks

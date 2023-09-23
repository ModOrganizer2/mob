#include "pch.h"
#include "../core/process.h"
#include "tools.h"

namespace mob {

    extractor::extractor() : basic_process_runner("extract") {}

    fs::path extractor::binary()
    {
        return conf().tool().get("sevenz");
    }

    extractor& extractor::file(const fs::path& file)
    {
        file_ = file;
        return *this;
    }

    extractor& extractor::output(const fs::path& dir)
    {
        where_ = dir;
        return *this;
    }

    void extractor::do_run()
    {
        interruption_file ifile(cx(), where_, "extractor");

        // check interruption file from last run
        if (ifile.exists()) {
            // resume the extraction, will overwrite
            cx().debug(context::generic,
                       "previous extraction was interrupted; resuming");
        }
        else if (fs::exists(where_)) {
            if (conf().global().reextract()) {
                // output already exists, no interruption file, but the user wants
                // to re-extract
                cx().debug(context::reextract, "deleting {}", where_);
                op::delete_directory(cx(), where_, op::optional);
            }
            else {
                // output already exists, no interruption file, assume it's fine
                cx().debug(context::bypass, "directory {} already exists", where_);
                return;
            }
        }

        cx().debug(context::generic, "extracting {} into {}", file_, where_);

        op::create_directories(cx(), where_);

        // will be left on disk on crashes or interruptions
        ifile.create();

        // deletes the directory in the destructor if in case of hard failure, but
        // not interruptions so extraction is resumed later
        directory_deleter delete_output(cx(), where_);

        // some archives have a top-level directory, others have files directly in
        // it, and it sucks to have special cases that know about individual
        // third parties, so this tries to figure out whether to move the files
        // after extraction
        //
        // now, the -spe flag from 7z is supposed to figure out if there's a folder
        // in the archive with the same name as the target and extract its content
        // to avoid duplicating the folder
        //
        // however, it fails miserably if there are files along with that folder,
        // which is the case for openssl:
        //
        //  openssl-1.1.1d.tar/
        //   +- openssl-1.1.1d/
        //   +- pax_global_header
        //
        // that pax_global_header makes 7z fail with "unspecified error", so -spe
        // just can't be used at all
        //
        // so the handling of a duplicate directory is done manually in
        // check_duplicate_directory() below, unfortunately

        if (file_.u8string().ends_with(u8".tar.gz")) {
            // tar in gz, must be piped, 7z can't do it in one step

            cx().trace(context::generic, "this is a tar.gz, piping");

            // untar
            auto extract_tar = process()
                                   .binary(binary())
                                   .arg("x")     // extract
                                   .arg("-so")   // output to stdout
                                   .arg(file_);  // input file

            // decompress
            auto extract_gz = process()
                                  .binary(binary())
                                  .arg("x")      // extract
                                  .arg("-aoa")   // overwrite all without prompt
                                  .arg("-si")    // read from stdin
                                  .arg("-ttar")  // type is tar
                                  .arg("-o", where_, process::nospace);  // output file

            auto piped = process::pipe(extract_tar, extract_gz);
            execute_and_join(piped);
        }
        else {
            // not tar, just extract directly

            execute_and_join(process()
                                 .binary(binary())
                                 .arg("x")     // extract
                                 .arg("-aoa")  // overwrite all without prompt
                                 .arg("-bd")   // no progress indicator
                                 .arg("-bb0")  // disable log
                                 .arg("-o", where_, process::nospace)  // output file
                                 .arg(file_));                         // input file
        }

        // moves files up if necessary
        check_for_top_level_directory(ifile.file());

        // success or interruption, don't delete the directory
        delete_output.cancel();

        if (!interrupted()) {
            // extraction finished and not interrupted, everything worked, so remove
            // the interruption file
            ifile.remove();
        }
    }

    void extractor::check_for_top_level_directory(const fs::path& ifile)
    {
        const auto dir_name = where_.filename();

        // check for a folder with the same name
        if (!fs::exists(where_ / dir_name)) {
            cx().trace(context::generic, "no duplicate subdir {}, leaving as-is",
                       dir_name);

            return;
        }

        cx().trace(context::generic,
                   "found subdir {} with same name as output dir; "
                   "moving everything up one",
                   dir_name);

        // the archive contained a directory with the same name as the output
        // directory

        // delete anything other than this directory; some archives have
        // useless files along with it
        for (auto e : fs::directory_iterator(where_)) {
            // but don't delete the directory itself
            if (e.path().filename() == dir_name)
                continue;

            // or the interrupt file
            if (e.path().filename() == ifile.filename())
                continue;

            if (!fs::is_regular_file(e.path())) {
                // don't know what to do with archives that have the
                // same directory _and_ other directories, bail out for now
                cx().bail_out(context::generic,
                              "check_duplicate_directory: {} is yet another directory",
                              e.path());
            }

            cx().trace(context::generic, "assuming file {} is useless, deleting",
                       e.path());

            op::delete_file(cx(), e.path());
        }

        // now there should only be two things in this directory: another
        // directory with the same name and the interrupt file

        // give it a temp name in case there's yet another directory with the
        // same name in it
        const auto temp_dir = where_ / (u8"_mob_" + dir_name.u8string());

        cx().trace(context::generic, "renaming dir to {} to avoid clashes", temp_dir);

        if (fs::exists(temp_dir)) {
            cx().trace(context::generic, "temp dir {} already exists, deleting",
                       temp_dir);

            op::delete_directory(cx(), temp_dir);
        }

        op::rename(cx(), where_ / dir_name, temp_dir);

        // move the content of the directory up
        for (auto e : fs::directory_iterator(temp_dir))
            op::move_to_directory(cx(), e.path(), where_);

        // delete the old directory, which should be empty now
        op::delete_directory(cx(), temp_dir);
    }

    void archiver::create_from_glob(const context& cx, const fs::path& out,
                                    const fs::path& glob,
                                    const std::vector<std::string>& ignore)
    {
        op::create_directories(cx, out.parent_path());

        auto p = process()
                     .binary(extractor::binary())
                     .arg("a")      // add to archive
                     .arg(out)      // output file
                     .arg("-r")     // recursive
                     .arg("-mx=5")  // normal compression level
                     .arg(glob);    // input file

        for (auto&& i : ignore) {
            // x: exclude
            // r: recurse
            // !: filename or glob
            p.arg("-xr!", i, process::nospace);
        }

        p.run();
        p.join();
    }

    void archiver::create_from_files(const context& cx, const fs::path& out,
                                     const std::vector<fs::path>& files,
                                     const fs::path& files_root)
    {
        std::string list_file_text;
        std::error_code ec;

        // make each file relative to files_root, convert to utf8 and put in
        // list_file_text separated by newlines
        for (auto&& f : files) {
            fs::path rf = fs::relative(f, files_root, ec);

            if (ec) {
                cx.bail_out(context::fs, "file {} is not in root {}", f, files_root);
            }

            list_file_text += path_to_utf8(rf) + "\n";
        }

        const auto list_file = make_temp_file();

        // always delete the list file when done
        guard g([&] {
            if (fs::exists(list_file)) {
                std::error_code ec;
                fs::remove(list_file, ec);
            }
        });

        op::write_text_file(gcx(), encodings::utf8, list_file, list_file_text);
        op::create_directories(cx, out.parent_path());

        auto p = process()
                     .binary(extractor::binary())
                     .arg("a")  // add to archive
                     .arg(out)  // output file
                     .arg("@", list_file, process::nospace)
                     .cwd(files_root);

        p.run();
        p.join();
    }

}  // namespace mob

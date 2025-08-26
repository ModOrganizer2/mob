#include "pch.h"
#include "op.h"
#include "../tools/tools.h"
#include "../utility.h"
#include "conf.h"
#include "context.h"

namespace mob::op {

    // most of the functions from the header will check the paths, return early
    // for dry run, and then forward to these to do the actual work
    void do_touch(const context& cx, const fs::path& p);
    void do_create_directories(const context& cx, const fs::path& p);
    void do_delete_directory(const context& cx, const fs::path& p);
    void do_delete_file(const context& cx, const fs::path& p);
    void do_copy_file_to_dir(const context& cx, const fs::path& f, const fs::path& d);
    void do_copy_file_to_file(const context& cx, const fs::path& f, const fs::path& d);
    void do_remove_readonly(const context& cx, const fs::path& p);
    void do_rename(const context& cx, const fs::path& src, const fs::path& dest);

    // checks whether the path is valid, bails out if not
    //
    void check(const context& cx, const fs::path& p, flags f);

    void touch(const context& cx, const fs::path& p, flags f)
    {
        cx.trace(context::fs, "touching {}", p);
        check(cx, p, f);

        if (!conf().global().dry())
            do_touch(cx, p);
    }

    void create_directories(const context& cx, const fs::path& p, flags f)
    {
        cx.trace(context::fs, "creating dir {}", p);
        check(cx, p, f);

        if (!conf().global().dry())
            do_create_directories(cx, p);
    }

    void delete_directory(const context& cx, const fs::path& p, flags f)
    {
        cx.trace(context::fs, "deleting dir {}", p);
        check(cx, p, f);

        if (!fs::exists(p)) {
            if (f & optional) {
                cx.trace(context::fs, "not deleting dir {}, doesn't exist (optional)",
                         p);

                return;
            }

            cx.bail_out(context::fs, "can't delete dir {}, doesn't exist", p);
        }

        if (fs::exists(p) && !fs::is_directory(p))
            cx.bail_out(context::fs, "{} is not a dir", p);

        if (!conf().global().dry())
            do_delete_directory(cx, p);
    }

    void delete_file(const context& cx, const fs::path& p, flags f)
    {
        cx.trace(context::fs, "deleting file {}", p);
        check(cx, p, f);

        if (!fs::exists(p)) {
            if (f & optional) {
                cx.trace(context::fs, "not deleting file {}, doesn't exist (optional)",
                         p);

                return;
            }

            cx.bail_out(context::fs, "can't delete file {}, doesn't exist", p);
        }

        if (fs::exists(p) && !fs::is_regular_file(p)) {
            if (f & optional)
                cx.warning(context::fs, "can't delete {}, not a file", p);
            else
                cx.bail_out(context::fs, "can't delete {}, not a file", p);

            return;
        }

        if (!conf().global().dry())
            do_delete_file(cx, p);
    }

    void delete_file_glob(const context& cx, const fs::path& glob, flags f)
    {
        cx.trace(context::fs, "deleting glob {}", glob);

        const auto parent   = glob.parent_path();
        const auto wildcard = glob.filename().native();

        if (!fs::exists(parent))
            return;

        for (auto&& e : fs::directory_iterator(parent)) {
            const auto p    = e.path();
            const auto name = p.filename().native();

            if (!PathMatchSpecW(name.c_str(), wildcard.c_str())) {
                cx.trace(context::fs, "{} did not match {}; skipping", name, wildcard);

                continue;
            }

            delete_file(cx, p, f);
        }
    }

    void delete_file_glob_recurse(const context& cx, const fs::path& directory,
                                  const fs::path& glob, flags f)
    {
        cx.trace(context::fs, "deleting glob {}", glob);

        const auto native = glob.native();

        if (!fs::exists(directory))
            return;

        for (auto&& e : fs::recursive_directory_iterator(directory)) {
            const auto p    = e.path();
            const auto name = p.filename().native();

            if (!PathMatchSpecW(name.c_str(), native.c_str())) {
                cx.trace(context::fs, "{} did not match {}; skipping", name, glob);

                continue;
            }

            delete_file(cx, p, f);
        }
    }

    void remove_readonly(const context& cx, const fs::path& dir, flags f)
    {
        cx.trace(context::fs, "removing read-only from {}", dir);
        check(cx, dir, f);

        if (!conf().global().dry()) {
            for (auto&& p : fs::recursive_directory_iterator(dir)) {
                if (fs::is_regular_file(p))
                    do_remove_readonly(cx, p);
            }
        }
    }

    bool is_source_better(const context& cx, const fs::path& src, const fs::path& dest)
    {
        if (!fs::exists(dest)) {
            cx.trace(context::fs, "target {} doesn't exist; copying", dest);
            return true;
        }

        std::error_code ec;

        const auto src_size = fs::file_size(src, ec);
        if (ec) {
            cx.warning(context::fs, "failed to get size of {}, {}; forcing copy", src,
                       ec.message());

            return true;
        }

        const auto dest_size = fs::file_size(dest, ec);
        if (ec) {
            cx.warning(context::fs, "failed to get size of {}, {}; forcing copy", dest,
                       ec.message());

            return true;
        }

        if (src_size != dest_size) {
            cx.trace(context::fs, "src {} bytes, dest {} bytes; different, copying",
                     src, src_size, dest, dest_size);

            return true;
        }

        const auto src_time = fs::last_write_time(src, ec);
        if (ec) {
            cx.warning(context::fs, "failed to get time of {}, {}; forcing copy", src,
                       ec.message());

            return true;
        }

        const auto dest_time = fs::last_write_time(dest, ec);
        if (ec) {
            cx.warning(context::fs, "failed to get time of {}, {}; forcing copy", dest,
                       ec.message());

            return true;
        }

        if (src_time > dest_time) {
            cx.trace(context::fs, "src {} is newer than dest {}; copying", src, dest);

            return true;
        }

        // same size, same date
        return false;
    }

    void rename(const context& cx, const fs::path& src, const fs::path& dest, flags f)
    {
        check(cx, src, f);
        check(cx, dest, f);

        if (fs::exists(dest)) {
            cx.bail_out(context::fs, "can't rename {} to {}, already exists", src,
                        dest);
        }

        cx.trace(context::fs, "renaming {} to {}", src, dest);

        if (!conf().global().dry())
            do_rename(cx, src, dest);
    }

    void move_to_directory(const context& cx, const fs::path& src,
                           const fs::path& dest_dir, flags f)
    {
        check(cx, src, f);
        check(cx, dest_dir, f);

        const auto target = dest_dir / src.filename();

        if (fs::exists(target)) {
            cx.bail_out(context::fs, "can't move {} to directory {}, {} already exists",
                        src, dest_dir, target);
        }

        cx.trace(context::fs, "moving {} to {}", src, target);

        if (!conf().global().dry())
            do_rename(cx, src, target);
    }

    void copy_file_to_dir_if_better(const context& cx, const fs::path& file,
                                    const fs::path& dir, flags f)
    {
        check(cx, file, f);
        check(cx, dir, f);

        if (file.u8string().find(u8"*") != std::string::npos)
            cx.bail_out(context::fs, "{} contains a glob", file);

        if (!conf().global().dry()) {
            if (!fs::exists(file) || !fs::is_regular_file(file)) {
                if (f & optional) {
                    cx.trace(context::fs, "not copying {}, doesn't exist (optional)",
                             file);

                    return;
                }

                cx.bail_out(context::fs, "can't copy {}, not a file", file);
            }

            if (fs::exists(dir) && !fs::is_directory(dir))
                cx.bail_out(context::fs, "can't copy to {}, not a dir", dir);
        }

        const auto target = dir / file.filename();
        if (is_source_better(cx, file, target)) {
            cx.trace(context::fs, "{} -> {}", file, dir);

            if (!conf().global().dry())
                do_copy_file_to_dir(cx, file, dir);
        }
        else {
            cx.trace(context::bypass, "(skipped) {} -> {}", file, dir);
        }
    }

    void copy_file_to_file_if_better(const context& cx, const fs::path& src,
                                     const fs::path& dest, flags f)
    {
        check(cx, src, f);
        check(cx, dest, f);

        if (src.u8string().find(u8"*") != std::string::npos)
            cx.bail_out(context::fs, "{} contains a glob", src);

        if (!conf().global().dry()) {
            if (!fs::exists(src)) {
                if (f & optional) {
                    cx.trace(context::fs, "not copying {}, doesn't exist (optional)",
                             src);

                    return;
                }

                cx.bail_out(context::fs, "can't copy {}, doesn't exist", src);
            }

            if (fs::exists(dest) && fs::is_directory(dest)) {
                cx.bail_out(context::fs,
                            "can't copy to {}, already exists but is a directory",
                            dest);
            }
        }

        if (is_source_better(cx, src, dest)) {
            cx.trace(context::fs, "{} -> {}", src, dest);

            if (!conf().global().dry())
                do_copy_file_to_file(cx, src, dest);
        }
        else {
            cx.trace(context::bypass, "(skipped) {} -> {}", src, dest);
        }
    }

    void copy_glob_to_dir_if_better(const context& cx, const fs::path& src_glob,
                                    const fs::path& dest_dir, flags f)
    {
        check(cx, dest_dir, f);

        const auto file_parent = src_glob.parent_path();
        const auto wildcard    = src_glob.filename().native();

        if (!fs::exists(file_parent)) {
            cx.bail_out(context::fs,
                        "can't copy glob {} to {}, parent directory {} doesn't exist",
                        src_glob, dest_dir, file_parent);
        }

        for (auto&& e : fs::directory_iterator(file_parent)) {
            const auto name = e.path().filename().native();

            if (!PathMatchSpecW(name.c_str(), wildcard.c_str())) {
                cx.trace(context::fs, "{} did not match {}; skipping", name, wildcard);

                continue;
            }

            if (e.is_regular_file()) {
                if (f & copy_files) {
                    copy_file_to_dir_if_better(cx, e.path(), dest_dir);
                }
                else {
                    cx.trace(context::fs, "file {} matched {} but files are not copied",
                             name, wildcard);
                }
            }
            else if (e.is_directory()) {
                if (f & copy_dirs) {
                    const fs::path sub = dest_dir / e.path().filename();

                    create_directories(cx, sub);
                    copy_glob_to_dir_if_better(cx, e.path() / "*", sub, f);
                }
                else {
                    cx.trace(context::fs,
                             "directory {} matched {} but directories are not copied",
                             name, wildcard);
                }
            }
        }
    }

    void replace_file(const context& cx, const fs::path& src, const fs::path& dest,
                      const fs::path& backup, flags f)
    {
        cx.trace(context::fs, "swapping {} and {}", src, dest);

        check(cx, src, f);
        check(cx, dest, f);

        if (conf().global().dry())
            return;

        const wchar_t* backup_p = nullptr;
        std::wstring backup_s;

        if (!backup.empty()) {
            backup_s = backup.native();
            backup_p = backup_s.c_str();
        }

        const auto r = ::ReplaceFileW(
            src.native().c_str(), dest.native().c_str(), backup_p,
            REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS, nullptr,
            nullptr);

        if (r)
            return;

        const auto e = GetLastError();

        cx.warning(context::generic,
                   "failed to atomically rename {} to {}, {}; hoping for the best", src,
                   dest, error_message(e));

        op::rename(cx, src, backup);
        op::rename(cx, dest, src);
    }

    std::string read_text_file_impl(const context& cx, const fs::path& p, flags f)
    {
        cx.trace(context::fs, "reading {}", p);

        std::string s;
        std::ifstream in(p, std::ios::binary);

        in.seekg(0, std::ios::end);
        s.resize(static_cast<std::size_t>(in.tellg()));
        in.seekg(0, std::ios::beg);
        in.read(&s[0], static_cast<std::streamsize>(s.size()));

        if (in.bad()) {
            if (f & optional)
                cx.debug(context::fs, "can't read from {} (optional)", p);
            else
                cx.bail_out(context::fs, "can't read from {}", p);
        }
        else {
            cx.trace(context::fs, "finished reading {}, {} bytes", p, s.size());
        }

        return s;
    }

    std::string read_text_file(const context& cx, encodings e, const fs::path& p,
                               flags f)
    {
        std::string bytes = read_text_file_impl(cx, p, f);
        if (bytes.empty())
            return bytes;

        std::string utf8 = bytes_to_utf8(e, bytes);
        utf8             = replace_all(utf8, "\r\n", "\n");

        return utf8;
    }

    void write_text_file(const context& cx, encodings e, const fs::path& p,
                         std::string_view utf8, flags f)
    {
        const std::string bytes = utf8_to_bytes(e, utf8);
        cx.trace(context::fs, "writing {} bytes to {}", bytes.size(), p);

        check(cx, p, f);

        if (conf().global().dry())
            return;

        {
            std::ofstream out(p, std::ios::binary);
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            out.close();

            if (out.bad()) {
                if (f & optional) {
                    cx.debug(context::fs, "can't write to {} (optional)", p);
                }
                else {
                    cx.bail_out(context::fs, "can't write to {}", p);
                }
            }
        }

        cx.trace(context::fs, "finished writing {} bytes to {}", bytes.size(), p);
    }

    void archive_from_glob(const context& cx, const fs::path& src_glob,
                           const fs::path& dest_file,
                           const std::vector<std::string>& ignore, flags f)
    {
        cx.trace(context::fs, "archiving {} into {}", src_glob, dest_file);
        check(cx, dest_file, f);

        if (conf().global().dry())
            return;

        archiver::create_from_glob(cx, dest_file, src_glob, ignore);
    }

    void archive_from_files(const context& cx, const std::vector<fs::path>& files,
                            const fs::path& files_root, const fs::path& dest_file,
                            flags f)
    {
        check(cx, dest_file, f);

        cx.trace(context::fs, "archiving {} files rooted in {} into {}", files.size(),
                 files_root, dest_file);

        if (conf().global().dry())
            return;

        archiver::create_from_files(cx, dest_file, files, files_root);
    }

    void do_touch(const context& cx, const fs::path& p)
    {
        op::create_directories(cx, p.parent_path());

        std::ofstream out(p);
        if (!out)
            cx.bail_out(context::fs, "failed to touch {}", p);
    }

    void do_create_directories(const context& cx, const fs::path& p)
    {
        std::error_code ec;
        fs::create_directories(p, ec);

        if (ec)
            cx.bail_out(context::fs, "can't create {}, {}", p, ec.message());
    }

    void do_delete_directory(const context& cx, const fs::path& p)
    {
        std::error_code ec;
        fs::remove_all(p, ec);

        if (ec) {
            if (ec.value() == ERROR_ACCESS_DENIED) {
                cx.trace(context::fs,
                         "got access denied trying to delete dir {}, "
                         "trying to remove read-only flag recursively",
                         p);

                remove_readonly(cx, p);
                fs::remove_all(p, ec);

                if (!ec)
                    return;
            }

            cx.bail_out(context::fs, "failed to delete {}, {}", p, ec.message());
        }
    }

    void do_delete_file(const context& cx, const fs::path& p)
    {
        std::error_code ec;
        fs::remove(p, ec);

        if (ec)
            cx.bail_out(context::fs, "can't delete {}, {}", p, ec.message());
    }

    void do_copy_file_to_dir(const context& cx, const fs::path& f, const fs::path& d)
    {
        if (!fs::exists(d))
            op::create_directories(cx, d);

        std::error_code ec;
        fs::copy_file(f, d / f.filename(), fs::copy_options::overwrite_existing, ec);

        if (ec) {
            cx.bail_out(context::fs, "can't copy {} to {}, {}", f, d, ec.message());
        }
    }

    void do_copy_file_to_file(const context& cx, const fs::path& src,
                              const fs::path& dest)
    {
        op::create_directories(cx, dest.parent_path());

        std::error_code ec;
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);

        if (ec) {
            cx.bail_out(context::fs, "can't copy {} to {}, {}", src, dest,
                        ec.message());
        }
    }

    void do_remove_readonly(const context& cx, const fs::path& p)
    {
        cx.trace(context::fs, "chmod +x {}", p);

        std::error_code ec;
        fs::permissions(p, fs::perms::owner_write, fs::perm_options::add, ec);

        if (ec) {
            cx.bail_out(context::fs, "can't remove read-only flag on {}, {}", p,
                        ec.message());
        }
    }

    void do_rename(const context& cx, const fs::path& src, const fs::path& dest)
    {
        std::error_code ec;
        fs::rename(src, dest, ec);

        if (ec) {
            cx.bail_out(context::fs, "can't rename {} to {}, {}", src, dest,
                        ec.message());
        }
    }

    void check(const context& cx, const fs::path& p, flags f)
    {
        if (p.empty())
            cx.bail_out(context::fs, "path is empty");

        if (is_set(f, unsafe))
            return;

        auto is_inside = [](auto&& p, auto&& dir) {
            const std::string s      = path_to_utf8(p);
            const std::string prefix = path_to_utf8(dir);

            if (s.size() < prefix.size())
                return false;

            const std::string scut = s.substr(0, prefix.size());

            if (_stricmp(scut.c_str(), prefix.c_str()) != 0)
                return false;

            return true;
        };

        if (is_inside(p, conf().path().prefix()))
            return;

        if (is_inside(p, conf().path().temp_dir()))
            return;

        if (is_inside(p, conf().path().licenses()))
            return;

        cx.bail_out(context::fs, "path {} is outside prefix", p);
    }

}  // namespace mob::op

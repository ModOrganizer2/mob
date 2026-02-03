#pragma once

#include "../utility.h"

namespace mob {
    class context;
}

namespace mob::op {

    // filesystem operations, also handle --dry
    //
    // for functions that end with _if_better(): the source is considered better
    // than the destination if:
    //   1) the destination doesn't exist, or
    //   2) the size is different, or
    //   3) the date is newer

    // various flags for the operations below, only some of them are used by some
    // functions
    //
    enum flags {
        noflags = 0x00,

        // the operation is optional, don't bail out if it fails
        optional = 0x01,

        // used by copy_glob_to_dir_if_better() to decide if files and/or
        // directories are copied
        copy_files = 0x02,
        copy_dirs  = 0x04,

        // operations will typically fail early if paths are empty or if they're not
        // inside a list of approved locations, like the prefix, %TEMP%, etc.
        //
        // this is to prevent mob from going on a deletion spree in case of bugs
        unsafe = 0x08
    };

    MOB_ENUM_OPERATORS(flags);

    // creates the given file if it doesn't exist
    //
    void touch(const context& cx, const fs::path& p, flags f = noflags);

    // creates all the directories in the given path
    //
    void create_directories(const context& cx, const fs::path& p, flags f = noflags);

    // deletes the given directory, recursive
    //
    // if deletion fails because of access denied, attempts to remove the readonly
    // flag on all files and tries again; this happens with some archives like 7z
    //
    // if the directory is controlled by git, prefer git_wrap::delete_directory(),
    // which checks for uncommitted changes before
    //
    void delete_directory(const context& cx, const fs::path& p, flags f = noflags);

    // deletes the given file
    //
    void delete_file(const context& cx, const fs::path& p, flags f = noflags);

    // deletes all files matching the glob in the glob's parent directory
    //
    void delete_file_glob(const context& cx, const fs::path& glob, flags f = noflags);

    // deletes all files matching the glob in the given directory and its subdirectories
    //
    void delete_file_glob_recurse(const context& cx, const fs::path& directory,
                                  const fs::path& glob, flags f = noflags);

    // removes the readonly flag for all files in `dir`, recursive
    //
    void remove_readonly(const context& cx, const fs::path& dir, flags f = noflags);

    // renames `src` to `dest`, files or directories; fails if it already exists
    //
    void rename(const context& cx, const fs::path& src, const fs::path& dest,
                flags f = noflags);

    // moves a file or directory `src` into dir `dest_dir`, using the same name
    // (renames src to dest_dir/src.filename()); fails if it already exists
    //
    void move_to_directory(const context& cx, const fs::path& src,
                           const fs::path& dest_dir, flags f = noflags);

    // copies a single file `file` into `dest_dir`; if the file already exists, only
    // copies it if it's considered better (see comment on top); doesn't support
    // globs or directories
    //
    void copy_file_to_dir_if_better(const context& cx, const fs::path& file,
                                    const fs::path& dest_dir, flags f = noflags);

    // same as copy_file_to_dir_if_better(), but the `dest_file` contains the
    // target filename instead of being constructed from dest_dir/src.filename()
    //
    void copy_file_to_file_if_better(const context& cx, const fs::path& src_file,
                                     const fs::path& dest_file, flags f = noflags);

    // basically calls copy_file_to_dir_if_better() for every file matching the
    // glob; recursive
    //
    void copy_glob_to_dir_if_better(const context& cx, const fs::path& src_glob,
                                    const fs::path& dest_dir, flags f);

    // renames `dest` to `src`, deleting `src` if it exists; if `backup` is given,
    // `src` is first renamed to it
    //
    // this attempts an atomic rename with ReplaceFile(), falls back to non-atomic
    // renames if it fails
    //
    void replace_file(const context& cx, const fs::path& src, const fs::path& dest,
                      const fs::path& backup = {}, flags f = noflags);

    // reads the given file, converts it to utf8 from the given encoding, returns
    // the utf8 string; if `e` is `dont_know`, returns the bytes as-is
    //
    std::string read_text_file(const context& cx, encodings e, const fs::path& p,
                               flags f = noflags);

    // creates file `p`, writes the given utf8 string into it, converting the string
    // to the given encoding; if `e` is dont_know, the bytes are written as-is
    //
    void write_text_file(const context& cx, encodings e, const fs::path& p,
                         std::string_view utf8, flags f = noflags);

    // creates an archive `dest_file` and puts all the files matching `src_glob`
    // into it, ignoring any file in `ignore` by name
    //
    // uses tools::archiver
    //
    void archive_from_glob(const context& cx, const fs::path& src_glob,
                           const fs::path& dest_file,
                           const std::vector<std::string>& ignore, flags f = noflags);

    // creates an archive `dest_file` and puts all the files from `files` in it,
    // resolving relative paths against `files_root`
    //
    void archive_from_files(const context& cx, const std::vector<fs::path>& files,
                            const fs::path& files_root, const fs::path& dest_file,
                            flags f = noflags);

}  // namespace mob::op

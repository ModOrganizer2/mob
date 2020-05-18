## Quick start
```
> git clone https://github.com/isanae/mob
> bootstrap
> mob -d prefix/path build
```

## Changing options
`mob` has two ways of setting options: from an INI file, or from the command line. Options set on the command line will always override options from INI files.

### INI files
By default, `mob` will look for a master INI `mob.ini` in the current directory and up to three of its parents (so it can also be found from the build directory). Once `mob` found the master INI, it will also look for any other `.ini` file in the same directory.

These additional INI files will be loaded after the master, in lexicographical order, overriding anything the previous INI file may have set. This makes it easy to have a file `mob-mine.ini` next to `mob.ini` with only the settings that need to be set or overriden, such as the build prefix.

Additional INI files may be specified with `--ini`. They will be loaded in order after the ones that were auto detected. If the same INI file is found in the directory and on the command line, its position in the load order will be moved to that of the command line.

If `--no-default-inis` is given, `mob` will not look for any `.ini` files and will only rely on the file(s) given with `--ini`. In this case, the master INI should be given first.

Inside the INI file are `[sections]` and `key = value` pairs. The `[options]` section is special because it can be changed for specific tasks instead of globally. Any value under a `[task:options]` section will only apply to a task named `task`. The list of available tasks can be seen with `mob list`. The task `super` is a shortcut for any task that's from the `ModOrganizer2` repository, except for `libbsarch`, `NexusClientCli` and `usvfs`.

For example, in a file `mob-mine.ini` that resides next to `mob.ini`:

```
[paths]
prefix = C:\dev\modorganizer

[super:options]
git_username = isanae
git_email    = isanae@users.noreply.github.com
```

### Command line
Any option can be overriden from the command like with `-s task:section/key=value`, where `task:` is optional. Some options have shortcuts, such as `--dry` for `-s global/dry=true` and `-l5` for `-s global:output_log_level=5`.


## Options

### `[global]`
| Option             | Type | Description |
| ---                | ---  | ---         |
| `dry`              | bool | Whether filesystem operations are simulated. Note that many operations will fail and that the build process will most probably not complete. This is mostly useful to get a dump of the options. |
| `redownload`       | bool | For `build`, re-downloads archives even if they already exist. |
| `reextract`        | bool | For `build`, re-extracts archives even if the target directory already exists, in which case it is deleted first. |
| `rebuild`          | bool | For `build`, does its best to clean the directory of build files before building. Some tasks will delete the whole directory and re-extract. |
| `output_log_level` | [0-6]| The log level for stdout: 0=silent, 1=errors, 2=warnings, 3=info (default), 4=debug, 5=trace, 6=dump. Note that 6 will dump _a lot_ of stuff, such as debug information from curl during downloads.
| `file_log_level`   | [0-6]| The log level for the log file. |
| `log_file`         | path | The path to a log file. |

### `[options]`

#### Common git options
Unless otherwise stated, applies to any task that is a git repo.

| Option      | Type   | Description |
| ---         | ---    | ---         |
| `mo_org`    | string | The organization name when pulling from Github. Only applies to ModOrganizer projects, plus NCC and usvfs. |
| `mo_branch` | string | The branch name when pulling from Github. Only applies to ModOrganizer projects, plus NCC and usvfs. |
| `no_pull`   | bool   | If a repo is already cloned, a `git pull` will be done on it every time `mob build` is run. Set to `false` to never pull and build with whatever is in there. |
| `ignore_ts` | bool   | Marks all the `.ts` files in a repo with `--assume-unchanged`. Note that `mob git ignore-ts off` can be used to revert it. |

#### Git credentials
These are used to set `user.name` and `user.email`. Applies to any task that is a git repo.

| Option         | Type   | Description |
| ---            | ---    | --- |
| `git_username` | string | Username |
| `git_email`    | string | Email |

#### Origin and upstream remotes
When `git` clones, it automatically creates a remote named `origin` for the URL that was cloned. When `set_origin_remote` is `true`, this will:
 1) Rename `origin` to `upstream`, and
 2) Create a new `origin` with the given parameters.

| Option                       | Type   | Description |
| ---                          | ---    | --- |
| `set_origin_remote`          | bool   | Enables the feature |
| `remote_org`                 | string | Organization on Github for the new remote. The URL will be `git@github.com:org/git_file`, where `git_file` is the `repo.git` file for the current task. |
| `remote_key`                 | string | A PuTTY key, saved in `remote.origin.puttykeyfile`. Optional. |
| `remote_no_push_upstream`    | bool   | Sets the push URL for `upstream` to `nopushurl` to avoid accidental pushes. |
| `remote_push_default_origin` | bool   | Sets `origin` as the default push remote. |

For example, this will create an `origin` remote for all ModOrganizer2 projects and mark all `.ts` files as `--assume-unchanged`.

```
[super:options]
ignore_ts                  = true
git_username               = isanae
git_email                  = isanae@users.noreply.github.com
set_origin_remote          = true
remote_org                 = isanae
remote_key                 = private.ppk
remote_no_push_upstream    = true
remote_push_default_origin = true
```

### `[tools]`
The various tools in this section are used verbatim when creating processes and so will be looked in the `PATH` environment variable. `vcvars` is best left empty, it will be found using the `vswhere.exe` that's bundled as a third-party.

### `[prebuilt]`
Some tasks can use prebuilt binaries instead of building from source.

### `[versions]`
The versions for all the tasks.

### `[paths]`
The only path that's required is `prefix`, which is where `mob` will put everything. Within this directory will be `build/`, `downloads/` and `install/`. Everything else is derived from it.

If `mob` is unable to find the Qt installation directory, it can be specified in `qt_install`. This directory should contain `bin/`, `include/`, etc. It's typically something like `C:\Qt\5.14.2\msvc2017_64\`. The other path `qt_bin` will be derived from it, it's just `$qt_install/bin/`.

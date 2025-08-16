# `mob` - ModOrganizer Build Tool

This README explains how to use `mob` and build ModOrganizer2 with it.
If you want to contribute to a single plugin of MO2 or even write your own plugins,
check the [ModOrganizer2 Wiki](https://github.com/ModOrganizer2/modorganizer/wiki).

## Table of contents

- [Quick start](#quick-start)
- [Extended start](#extended-start)
- [Changing options](#changing-options)
  - [INI files](#override-options-using-ini-files)
  - [Command line](#override-options-using-command-line)
  - [INI format](#ini-format)
- [Options](#options)
  - [`[global]`](#global)
  - [`[task]`](#task)
  - [`[tools]`](#tools)
  - [`[versions]`](#versions)
  - [`[paths]`](#paths)
- [Command line](#command-line)
  - [Global options](#global-options)
  - [`build`](#build)
  - [`list`](#list)
  - [`options`](#options)
  - [`release`](#release)
  - [`git`](#git)
  - [`cmake-config`](#cmake-config)
  - [`inis`](#inis)

## Quick start

```powershell
git clone https://github.com/ModOrganizer2/mob
cd mob
./bootstrap
mob -d c:\somewhere build
```

## Extended start

### Qt Installation

Check [`mob.ini`](https://github.com/ModOrganizer2/mob/blob/master/mob.ini#L94) to find
out which version of Qt you need.

#### CLI based install using [aqt](https://github.com/miurahr/aqtinstall)

[aqt](https://github.com/miurahr/aqtinstall) is a CLI installer for Qt, it makes
installing Qt extremely quick and painless, and doesn't require a login.
Check the [documentation](https://aqtinstall.readthedocs.io/en/latest/installation.html)
to install **aqt** itself.

> [!IMPORTANT]
> As of version 3.3.0, **aqt** is unable to install Qt version 6.7.3 on the
> `win64_msvc2022_64` target architecture due to a non-standard download URL (see:
> miurahr/aqtinstall#919). The [Holt59/aqtinstall fork](https://github.com/Holt59/aqtinstall)
> provides a workaround for this issue and can be installed using the following command
> (requires Python 3.9 or above):
> ```powershell
> pip install git+https://github.com/Holt59/aqtinstall.git#egg=aqtinstall
> ```
> If you already installed **aqt** before, you first need to uninstall it before
> installing the fork:
> ```powershell
> pip uninstall aqtinstall
> ```

When using **aqt**, you can choose which modules to install but we recommend installing
all of them in case of changes.

```powershell
# you can also use -m all to install all modules
aqt install-qt --outputdir "C:\Qt" windows desktop ${QT_VERSION} win64_msvc2022_64 `
  -m qtwebengine qtimageformats qtpositioning qtserialport qtwebchannel qtwebsockets
```

#### Manual installation

- Install Qt  ([Installer](https://download.qt.io/official_releases/online_installers/qt-unified-windows-x64-online.exe)) and select these components:
  - MSVC 2022 64-bit
  - Additional Libraries:
    - Qt WebEngine (display nexus pages)
    - Qt Image Formats (display images in image tab and preview)
    - Qt Positioning (required by QtWebEngine)
    - Qt Serial Port (required by Qt Core)
    - Qt WebChannel (required by QtWebEngine)
    - Qt WebSockets (Nexus api/download)
  - Optional:
    - Qt Source Files
    - Qt Debug Files

### Visual Studio

- Install Visual Studio 2022 ([Installer](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&channel=Release&version=VS2022&source=VSLandingPage&cid=2030&passive=false))

  - Desktop development with C++
  - Desktop .NET desktop development (needed by OMOD and FOMOD installers)
  - Individual Components:
    - .Net Framework 4.8 SDK
    - .Net Framework 4.7.2 targeting pack  (OMOD targets 4.8 but VS still requires
      the package for other .Net components)
    - Windows Universal C Runtime
    - C++ ATL for latest v143 build Tools (x86 & x64)
    - C++ /CLI support for v143 build Tools (Latest)  (for OMOD and FOMOD installers)
    - Windows 11 SDK (get latest)
    - C++ Build Tools core features
    - Git for Windows (Skip if you have this already installed outside of the VS installer)
    - CMake tools for Windows (Skip if you have this already installed outside of the VS installer)

## Setting up MOB

```powershell
mkdir C:\dev
cd C:\dev

# clone this repository
git clone https://github.com/ModOrganizer2/mob

# build mob itself - this will create mob.exe in the current directory
./bootstrap.ps1

# build MO2 itself - this will take a LONG time
mob -d C:\dev\modorganizer build
```

Once `mob` is finished, everything will be in `C:\dev\modorganizer`.
Mod Organizer can be run from `install\bin\ModOrganizer.exe`.
The Visual Studio solution for Mod Organizer itself is `build\modorganizer_super\modorganizer\vsbuild\organizer.sln`.

## Changing options

`mob` has two ways of setting options: from INI files, the `MOBINI` environment
variable, or from the command line.

### Override options using INI files

`mob` builds a list of available INI files in order of priority. Higher numbers
override lower numbers:

 1) The master INI `mob.ini` in the directory where `mob.exe` lives (required).
 2) Any files set in `MOBINI` (separated by semicolons).
 3) Another `mob.ini` in the current directory.
 4) Files given with `--ini`.

Use `mob inis` to see the list of INI files in order. If `--no-default-inis` is given,
`mob` will skip 1) and 2). The first INI it finds after that is considered the master.

### Override options using command line

Any option can be overridden from the command like with `-s task:section/key=value`,
where `task:` is optional. Some options have shortcuts, such as `--dry`
for `-s global/dry=true` and `-l5` for `-s global:output_log_level=5`.
See `mob options` for the list of options.

### INI format

Inside the INI file are `[sections]` and `key = value` pairs. The `[task]` section is
special because it can be changed for specific tasks instead of globally.
Any value under a `[task_name:task]` section will only apply to a task
named `task_name`. The list of available tasks can be seen with `mob list`.
See [Task names](#task-names).

## Options

### `[global]`

| Option             | Type | Description |
| ---                | ---  | ---         |
| `dry`              | bool | Whether filesystem operations are simulated. Note that many operations will fail and that the build process will most probably not complete. This is mostly useful to get a dump of the options. |
| `redownload`       | bool | For `build`, re-downloads archives even if they already exist. |
| `reextract`        | bool | For `build`, re-extracts archives even if the target directory already exists, in which case it is deleted first. |
| `reconfigure`      | bool | For `build`, tries to delete just enough so that configure tools (such as cmake) will run from scratch. |
| `rebuild`          | bool | For `build`, tries to delete just enough so that build tools (such as msbuild) will run from scratch. |
| `clean_task`       | bool | For `build`, whether tasks are cleaned. |
| `fetch_task`       | bool | For `build`, whether tasks are fetched (download, git, etc.) |
| `build_task`       | bool | For `build`, whether tasks are built (msbuild, jobm etc.) |
| `output_log_level` | [0-6]| The log level for stdout: 0=silent, 1=errors, 2=warnings, 3=info (default), 4=debug, 5=trace, 6=dump. Note that 6 will dump _a lot_ of stuff, such as debug information from curl during downloads. |
| `file_log_level`   | [0-6]| The log level for the log file. |
| `log_file`         | path | The path to a log file. |
| `ignore_uncommitted` | bool | When `--redownload` or `--reextract` is given, directories controlled by git will be deleted even if they contain uncommitted changes.|

### `[task]`

Options for individual tasks. Can be `[task_name:task]`, where `task_name` is the name of a task (see `mob list`) , `super` for all MO tasks or a glob like `installer_*`.

| Option          | Type   | Description |
| ---             | ---    | ---         |
| `enabled`       | bool   | Whether this task is enabled. Disabled tasks are never built. When specifying task names with `mob build task1 task2...`, all tasks except those given are turned off. |
| `configuration` | enum   | Which configuration to build, should be one of Debug, Release or RelWithDebInfo with RelWithDebInfo being the default.|

#### Common git options

Unless otherwise stated, applies to any task that is a git repo.

| Option      | Type   | Description |
| ---         | ---    | ---         |
| `mo_org`    | string | The organisation name when pulling from Github. Only applies to ModOrganizer projects, plus NCC and usvfs. |
| `mo_branch` | string | The branch name when pulling from Github. Only applies to ModOrganizer projects, plus NCC and usvfs. |
| `mo_master` | string | The fallback branch name when pulling from Github. Only applies to ModOrganizer projects, plus NCC and usvfs. This branch is used when `mo_branch` does not exists. If this value is empty, the fallback mechanism is disabled (default behavior). |
| `no_pull`   | bool   | If a repo is already cloned, a `git pull` will be done on it every time `mob build` is run. Set to `false` to never pull and build with whatever is in there. |
| `ignore_ts` | bool   | Marks all the `.ts` files in a repo with `--assume-unchanged`. Note that `mob git ignore-ts off` can be used to revert it. |
| `git_url_prefix` | string | When cloning a repo, the URL will be `$(git_url_prefix)mo_org/repo.git`. |
| `git_shallow` | bool | When true, clones with `--depth 1` to avoid having to fetch all the history. Defaults to true for third-parties. |

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
| `remote_org`                 | string | Organisation on Github for the new remote. The URL will be `git@github.com:org/git_file`, where `git_file` is the `repo.git` file for the current task. |
| `remote_key`                 | string | A PuTTY key, saved in `remote.origin.puttykeyfile`. Optional. |
| `remote_no_push_upstream`    | bool   | Sets the push URL for `upstream` to `nopushurl` to avoid accidental pushes. |
| `remote_push_default_origin` | bool   | Sets `origin` as the default push remote. |

For example, this will create an `origin` remote for all ModOrganizer2 projects and mark all `.ts` files as `--assume-unchanged`.

```ini
[super:task]
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

### `[versions]`

The versions for all the tasks.

### `[paths]`

The only path that's required is `prefix`, which is where `mob` will put everything. Within this directory will be `build/`, `downloads/` and `install/`. Everything else is derived from it.

If `mob` is unable to find the Qt installation directory, it can be specified in `qt_install`. This directory should contain `bin/`, `include/`, etc.
It's typically something like `C:\Qt\6.7.3\msvc2022_64\`. The other path `qt_bin` will be derived from it, it's just `$qt_install/bin/`.

## Command line

Do `mob --help` for global options and the list of available commands. Do `mob <command> --help` for more help about a command.

To use global options with command options, ensure command options are together, with no global options in the middle.

### Global options

| Option              | Description |
| ---                 | --- |
| `--ini`             | Adds an INI file, see [INI files](#override-options-using-ini-files). |
| `--dry`             | Simulates filesystem operations. Note that many operations will fail and that the build process will stop with errors. This is mostly useful to get a dump of the options. |
| `-log-level`        | The log level for stdout: 0=silent, 1=errors, 2=warnings, 3=info (default), 4=debug, 5=trace, 6=dump. Note that 6 will dump _a lot_ of stuff, such as debug information from curl during downloads. |
| `--destination`     | The build directory where `mob` will put everything. |
| `--set`             | Sets an option: `-s task:section/key=value`. |
| `--no-default-inis` | Does not auto detect INI files, only uses `--ini`. |

### `build`

Builds tasks. The order in which tasks have to be built is handled by `mob`, but dependencies will not be built automatically when specifying tasks manually. That is, `mob build` will build `python` before `pyqt`, but `mob build pyqt` will not build `python`. Many tasks will be able to run in parallel, but not all, either because they hog the CPU (such as `usvfs`) or because they have dependencies that have to be built first.

If any task fails to build, all the active tasks are aborted as quickly as possible.

#### Task names

Each task has a name, some have more. MO tasks for example have a full name that corresponds to their git repo (such as `modorganizer-game_features`) and a shorter name (such as `game_features`). Both can be used interchangeably. The task name can also be `super`, which refers to all repos hosted on the Mod Organizer Github account, minus `libbsarch`, `usvfs` and `NexusClientCli`. Globs can be used, like `installer_*`. See `mob list` for a list of all available tasks.

#### Options for `build`

| Option | Description |
| ---    | --- |
| `--redownload`                       | Re-downloads files. If a download file is found in `prefix/downloads`, it is never re-downloaded. This will delete the file and download it again. |
| `--reextract`                        | Deletes the source directory for a task and re-extracts archives. If the directory is controlled by git, deletes it and clones again. If git finds modifications in the directory, the operation is aborted (see `--ignore-uncommitted-changes`. |
| `--reconfigure`                      | Reconfigures the task by running cmake, configure scripts, etc. Some tasks might have to delete the whole source directory. |
| `--rebuild`                          | Cleans and rebuilds projects. Some tasks might have to delete the whole source directory |
| `--new`                              | Implies all the four flags above. |
| `--clean-task`, `--no-clean-task` | Sets whether tasks are cleaned. With `--no-clean-task`, the flags above are ignored. |
| `--fetch-task`, `--no-fetch-task` | Sets whether tasks are fetched. With `--no-fetch-task`, nothing is downloaded, extracted, cloned or pulled. |
| `--build-task`, `--no-build-task` | Sets whether tasks are built. With `--no-build-task`, nothing is ever built or installed. |
| `--pull`, `--no-pull`             | For repos that are controlled by git, whether to pull repos that are already cloned. With `--no-pull`, once a repo is cloned, it is never updated automatically. |
| `--revert-ts`, `--no-revert-ts`   | Most projects will generate `.ts` files for translations. These files are typically not committed to Github and so will often conflict when trying to pull. With `--revert-ts`, any `.ts` file is reverted before pulling. |
| `--ignore-uncommitted-changes`       | With `--reextract`, ignores repos that have uncommitted changes and deletes the directory without confirmation. |
| `--keep-msbuild`                     | `mob` starts a lot of `msbuild.exe` processes, some of which hold locks on the build directory. Because that's pretty darn annoying, `mob` will kill all `msbuild.exe` processes when it finished, unless this flag is given. |
| `<task>...`                          | List of tasks to run, see [Task names](#task-names). |

### `list`

Lists all the available task names. If a task has multiple names, they are all shown, separated by a comma.

#### Options for `list`

| Option | Description |
| --- | --- |
| `--all`     | Shows a task tree to see which are built in parallel. |
| `<task>...` | This is the same list of tasks that can be given in the `build` command. With `--all`, this will only show the tasks that would be built. |

### `options`

Lists all the options after parsing the INIs and the command line.

### `release`

Creates a release in `prefix/releases`. Only supports devbuilds for now. A release is made out of three archives:

- Binaries from `prefix/install/bin`;
- PDBs from `prefix/install/pdb`;
- Sources from various directories in `prefix/build`.

The archive filename is `Mod.Organizer-version-suffix-what.7z`, where:

- `version` is taken from `ModOrganizer.exe`, `version.rc` or from `--version`;
- `suffix` is the optional `--suffix` argument;
- `what` is either nothing, `src` or `pdbs`.

#### Options for `release`

| Option | Description |
| --- | --- |
| `--bin`, `--no-bin`   | Whether the binary archive is created [default: yes] |
| `--pdbs`, `--no-pdbs` | Whether the PDBs archive is created [default: yes] |
| `--src`,, `--no-src`   | Whether the source archive is created [default: yes] |
| `--version-from-exe`     | Retrieves version information from ModOrganizer.exe [default] |
| `--version-from-rc`      | Retrieves version information from `modorganizer/src/version.rc` |
| `--rc <PATH>`            | Overrides the path to `version.rc` |
| `--version <VERSION>`    | Overrides the version string, ignores `--version-from-exe` and `--version-from-rc` |
| `--output-dir <PATH>`    | Sets the output directory to use instead of `prefix/releases` |
| `--suffix <SUFFIX>`      | Optional suffix to add to the archive filenames. |
| `--force`                | `mob` will refuse to create a source archive over 20MB because it would probably be incorrect. This ignores the file size warnings and creates the archive regardless of its size. |

### `git`

Various commands to manage the git repos. Includes `usvfs`, `NexusClientCli` and all the projects under `modorganizer_super`.

#### `set-remotes`

Does the same thing as the when `set_origin_remotes` is set in the INI: renames `origin` to `upstream` and adds a new `origin` with the options below. See [Origin and upstream remotes](#origin-and-upstream-remotes).

| Option | Description |
| --- | --- |
| `--username <USERNAME>` | Git username. |
| `--email <EMAIL>`       | Git email. |
| `--key <PATH>`          | Path to a putty key. |
| `--no-push`             | Disables pushing to `upstream` by changing the push url to `nopushurl` to avoid accidental pushes. |
| `--push-origin`         | Sets the new `origin` remote as the default push target. |
| `<path>`                | Only use this repo instead of going through all of them. |

#### `add-remote`

Simply adds a new remote with the given parameters to all the git repos.

| Option | Description |
| --- | --- |
| `-name <NAME>`          | Name of new remote |
| `--username <USERNAME>` | Git username |
| `--key <PATH>`          | Path to a putty key |
| `--push-origin`         | Sets this new remote as the default push target |
| `<path>`                | Only use this repo instead of going through all of them |

### `cmake-config`

The `cmake-config` command can display the `CMAKE_INSTALL_PREFIX` and
`CMAKE_PREFIX_PATH` variable using by `mob` itself when building so that you can run
your own `cmake`.

Below is a typical use of the command:

```powershell
# configure the current project with preset vs2022-windows using mob PATH
cmake --preset vs2022-windows `
  ("-DCMAKE_INSTALL_PREFIX=" + (mob cmake-config install-prefix)) `
  ("-DCMAKE_PREFIX_PATH=" + (mob cmake-config prefix-path))
```

### `inis`

Shows a list of the all the INIs that would be loaded, in order of priority.
See [INI files](#override-options-using-ini-files).

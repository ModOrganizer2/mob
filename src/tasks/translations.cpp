#include "pch.h"
#include "tasks.h"
#include "../core/env.h"
#include "../utility/threading.h"

namespace mob
{

translations::projects::lang::lang(std::string n)
	: name(std::move(n))
{
}

translations::projects::project::project(std::string n)
	: name(std::move(n))
{
}


translations::projects::projects(fs::path root)
	: root_(std::move(root))
{
	create();
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

void translations::projects::create()
{
	for (auto e : fs::directory_iterator(root_))
	{
		if (!e.is_directory())
			continue;

		handle_project_dir(e.path());
	}
}

bool translations::projects::is_gamebryo_plugin(
	const std::string& dir, const std::string& project)
{
	auto tasks = find_tasks(project);
	if (tasks.empty())
	{
		warnings_.push_back(::fmt::format(
			"directory '{}' was parsed as project '{}', but there's "
			"no task with this name", dir, project));

		return false;
	}

	const task& t = *tasks[0];

	if (!t.is_super())
		return false;

	const auto& mo_task = static_cast<const modorganizer&>(t);
	return mo_task.is_gamebryo_plugin();
}

void translations::projects::handle_project_dir(const fs::path& dir)
{
	const auto dir_name = path_to_utf8(dir.filename());
	const auto dir_cs = split(dir_name, ".");

	if (dir_cs.size() != 2)
	{
		warnings_.push_back(::fmt::format(
			"bad directory name '{}'; skipping", dir_name));

		return;
	}

	const auto project_name = trim_copy(dir_cs[1]);
	if (project_name.empty())
	{
		warnings_.push_back(::fmt::format(
			"bad directory name '{}', skipping", dir_name));

		return;
	}

	project p(project_name);

	const bool gamebryo = is_gamebryo_plugin(dir_name, project_name);

	for (auto f : fs::directory_iterator(dir))
	{
		if (!f.is_regular_file())
			continue;

		p.langs.push_back(handle_ts_file(gamebryo, project_name, f.path()));
	}

	projects_.push_back(p);
}

translations::projects::lang translations::projects::handle_ts_file(
	bool gamebryo, const std::string& project_name, const fs::path& f)
{
	lang lg(path_to_utf8(f.stem()));

	lg.ts_files.push_back(f);

	if (gamebryo)
	{
		const fs::path gamebryo_dir =
			conf::get_global("transifex", "project") + "." +
			"game_gamebryo";

		const auto gb_f = root_ / gamebryo_dir / f.filename();

		if (fs::exists(gb_f))
		{
			lg.ts_files.push_back(gb_f);
		}
		else
		{
			if (!warned_.contains(gb_f))
			{
				warned_.insert(gb_f);

				warnings_.push_back(::fmt::format(
					"{} is a gamebryo plugin but there is no '{}'; the "
					".qm file will be missing some translations (will "
					"only warn once)",
					project_name, path_to_utf8(gb_f)));
			}
		}
	}

	return lg;
}


translations::translations()
	: basic_task("translations")
{
}

bool translations::prebuilt()
{
	return false;
}

std::string translations::version()
{
	return {};
}

fs::path translations::source_path()
{
	return paths::build() / "transifex-translations";
}

void translations::do_clean(clean c)
{
	instrument<times::clean>([&]
	{
		if (is_set(c, clean::redownload))
			op::delete_directory(cx(), source_path(), op::optional);

		if (is_set(c, clean::rebuild))
		{
			op::delete_file_glob(
				cx(), paths::install_translations() / "*.qm", op::optional);
		}
	});
}

void translations::do_fetch()
{
	instrument<times::fetch>([&]
	{
		const url u =
			conf::get_global("transifex", "url") + "/" +
			conf::get_global("transifex", "team") + "/" +
			conf::get_global("transifex", "project");

		const std::string key = conf::get_global("transifex", "key");

		if (key.empty() && !this_env::get_opt("TX_TOKEN"))
		{
			cx().warning(context::generic,
				"no key was in the INI and the TX_TOKEN env variable doesn't "
				"exist, this will probably fail");
		}

		cx().debug(context::generic, "init tx");
		run_tool(transifex(transifex::init)
			.root(source_path()));

		if (conf::get_global_bool("transifex", "configure"))
		{
			cx().debug(context::generic, "configuring");
			run_tool(transifex(transifex::config)
				.root(source_path())
				.api_key(key)
				.url(u));
		}
		else
		{
			cx().trace(context::generic, "skipping configuring");
		}

		if (conf::get_global_bool("transifex", "pull"))
		{
			cx().debug(context::generic, "pulling");
			run_tool(transifex(transifex::pull)
				.root(source_path())
				.api_key(key)
				.minimum(conf::get_global_int("transifex", "minimum"))
				.force(conf::get_global_bool("transifex", "force")));
		}
		else
		{
			cx().trace(context::generic, "skipping pulling");
		}
	});
}

void translations::do_build_and_install()
{
	instrument<times::build>([&]
	{
		const auto root = source_path() / "translations";
		const auto dest = paths::install_translations();
		const projects ps(root);

		op::create_directories(cx(), dest);

		for (auto&& w : ps.warnings())
			cx().warning(context::generic, "{}", w);

		thread_pool tp;

		for (auto& p : ps.get())
		{
			for (auto& lg : p.langs)
			{
				tp.add([&]
				{
					threaded_run(lg.name + "." + p.name, [&]
					{
						run_tool(lrelease()
							.project(p.name)
							.sources(lg.ts_files)
							.out(dest));
					});
				});
			}
		}
	});
}

}	// namespace

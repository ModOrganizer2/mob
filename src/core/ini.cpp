#include "pch.h"
#include "ini.h"
#include "paths.h"
#include "context.h"
#include "env.h"
#include "conf.h"
#include "../tasks/task_manager.h"
#include "../utility/string.h"

namespace mob
{

template <class... Args>
void ini_error(
	const ini_data& ini, std::size_t line, std::string_view f, Args&&... args)
{
	gcx().bail_out(context::conf,
		"{}:{}: {}",
		path_to_utf8(ini.path), (line + 1),
		fmt::format(f, std::forward<Args>(args)...));
}


ini_data::kv_map& ini_data::get_section(std::string_view name)
{
	for (auto itor=sections.begin(); itor!=sections.end(); ++itor)
	{
		if (itor->first == name)
			return itor->second;
	}

	sections.push_back({std::string(name), kv_map()});
	return sections.back().second;
}

void ini_data::set(std::string_view section, std::string key, std::string value)
{
	auto& s = get_section(section);
	s.emplace(std::move(key), std::move(value));
}



std::string default_ini_filename()
{
	return "mob.ini";
}

std::vector<fs::path> find_inis(
	bool auto_detect, const std::vector<std::string>& from_cl, bool verbose)
{
	// the string is just for verbose
	std::vector<std::pair<std::string, fs::path>> v;

	// adds a path to the vector; if the path already exists, moves it to
	// the last element
	auto add_or_move_up = [&](std::string where, fs::path p)
	{
		for (auto itor=v.begin(); itor!=v.end(); ++itor)
		{
			if (fs::equivalent(p, itor->second))
			{
				auto pair = std::move(*itor);
				v.erase(itor);
				v.push_back({where + ", was " + pair.first, p});
				return;
			}
		}

		v.push_back({where, p});
	};

	// whether the ini is already in the list
	auto ini_already_found = [&](auto&& p)
	{
		for (auto itor=v.begin(); itor!=v.end(); ++itor)
		{
			if (fs::equivalent(p, itor->second))
				return true;
		}

		return false;
	};


	fs::path master;

	// auto detect from exe directory
	if (auto_detect)
	{
		if (verbose)
		{
			const auto r = find_root(verbose);
			u8cout << "root is " << path_to_utf8(r) << "\n";
		}

		master = find_in_root(default_ini_filename());

		if (verbose)
			u8cout << "found master " << path_to_utf8(master) << "\n";

		v.push_back({"master", master});
	}


	// MOBINI environment variable
	if (auto e=this_env::get_opt("MOBINI"))
	{
		if (verbose)
			u8cout << "found env MOBINI: '" << *e << "'\n";

		for (auto&& i : split(*e, ";"))
		{
			auto p = fs::path(i);
			if (!fs::exists(p))
			{
				u8cerr << "ini from env MOBINI " << i << " not found\n";
				throw bailed();
			}

			p = fs::canonical(p);

			if (verbose)
				u8cout << "ini from env: " << path_to_utf8(p) << "\n";

			add_or_move_up("env", p);
		}
	}


	// auto detect from the current directory
	if (auto_detect)
	{
		MOB_ASSERT(!master.empty());

		auto cwd = fs::current_path();

		while (!cwd.empty())
		{
			const auto in_cwd = cwd / default_ini_filename();

			if (fs::exists(in_cwd) && !ini_already_found(in_cwd))
			{
				if (verbose)
					u8cout << "also found in cwd " << path_to_utf8(in_cwd) << "\n";

				v.push_back({ "cwd", fs::canonical(in_cwd) });
				break;
			}

			const auto parent = cwd.parent_path();
			if (cwd == parent)
				break;

			cwd = parent;
		}
	}


	// command line
	for (auto&& i : from_cl)
	{
		auto p = fs::path(i);
		if (!fs::exists(p))
		{
			u8cerr << "ini " << i << " not found\n";
			throw bailed();
		}

		p = fs::canonical(p);

		if (verbose)
			u8cout << "ini from command line: " << path_to_utf8(p) << "\n";

		add_or_move_up("cl", p);
	}


	if (verbose)
	{
		u8cout << "\nhigher number overrides lower\n";

		for (std::size_t i=0; i<v.size(); ++i)
		{
			u8cout
				<< "  " << (i + 1) << ") "
				<< path_to_utf8(v[i].second) << " (" << v[i].first << ")\n";
		}
	}


	return map(v, [&](auto&& p){ return p.second; });
}

std::vector<std::string> read_ini(const fs::path& ini)
{
	std::ifstream in(ini);

	std::vector<std::string> lines;

	for (;;)
	{
		std::string line;
		std::getline(in, line);
		trim(line);

		if (!in)
			break;

		lines.push_back(std::move(line));
	}

	if (in.bad())
		gcx().bail_out(context::conf, "failed to read ini {}", ini);

	return lines;
}

void parse_line(
	ini_data& ini, std::size_t i, const std::string& line,
	const std::string& task, const std::string& section)
{
	auto& tm = task_manager::instance();

	const auto sep = line.find("=");
	if (sep == std::string::npos)
		ini_error(ini, i, "bad line '{}'", line);

	const std::string k = trim_copy(line.substr(0, sep));
	const std::string v = trim_copy(line.substr(sep + 1));

	if (k.empty())
		ini_error(ini, i, "bad line '{}'", line);

	if (section == "aliases")
	{
		tm.add_alias(k, split_quoted(v, " "));
	}
	else if (task.empty())
	{
		ini.set(section, k, v);
	}
	else
	{
		if (!tm.valid_task_name(task))
			ini_error(ini, i, "no task matching '{}' found", task);

		ini.set(task + ":" + section, k, v);
	}
}

void parse_section(
	ini_data& ini, std::size_t& i, const std::vector<std::string>& lines,
	const std::string& section_string)
{
	std::string task, section;

	const auto col = section_string.find(":");

	if (col == std::string::npos)
	{
		section = section_string;
	}
	else
	{
		task = section_string.substr(0, col);
		section = section_string.substr(col + 1);
	}


	++i;

	for (;;)
	{
		if (i >= lines.size() || lines[i][0] == '[')
			break;

		const auto& line = lines[i];

		// empty or comment
		if (line.empty() || line[0] == '#' || line[0] == ';')
		{
			++i;
			continue;
		}

		parse_line(ini, i, line, task, section);
		++i;
	}
}

ini_data parse_ini(const fs::path& path)
{
	gcx().debug(context::conf, "using ini at {}", path);

	ini_data ini;
	ini.path = path;

	const auto lines = read_ini(path);
	std::size_t i = 0;

	for (;;)
	{
		if (i >= lines.size())
			break;

		const auto& line = lines[i];

		// empty or comment
		if (line.empty() || line[0] == '#' || line[0] == ';')
		{
			++i;
			continue;
		}

		if (line.starts_with("[") && line.ends_with("]"))
		{
			const std::string name = line.substr(1, line.size() - 2);
			parse_section(ini, i, lines, name);
		}
		else
		{
			ini_error(ini, i, "bad line '{}'", line);
		}
	}

	return ini;
}

}	// namespace

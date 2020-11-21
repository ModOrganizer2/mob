#include "pch.h"
#include "ini.h"
#include "context.h"
#include "env.h"
#include "conf.h"
#include "../tasks/tasks.h"
#include "../utility/string.h"

namespace mob
{

std::string default_ini_filename()
{
	return "mob.ini";
}

parsed_option parse_option(const std::string& s)
{
	// task:section/key=value
	// task: is optional
	std::regex re(R"((?:(.+)\:)?(.+)/(.*)=(.*))");
	std::smatch m;

	if (!std::regex_match(s, m, re))
	{
		gcx().bail_out(context::conf,
			"bad option {}, must be [task:]section/key=value", s);
	}

	std::string task = trim_copy(m[1].str());
	std::string section = trim_copy(m[2].str());
	std::string key = trim_copy(m[3].str());
	std::string value = trim_copy(m[4].str());

	return {task, section, key, value};
}



template <class... Args>
void ini_error(
	const fs::path& ini, std::size_t line, std::string_view f, Args&&... args)
{
	gcx().bail_out(context::conf,
		"{}:{}: {}",
		path_to_utf8(ini), (line + 1),
		fmt::format(f, std::forward<Args>(args)...));
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

void parse_section(
	const fs::path& ini, std::size_t& i,
	const std::vector<std::string>& lines,
	std::string_view task, std::string_view section, bool add)
{
	++i;

	for (;;)
	{
		if (i >= lines.size() || lines[i][0] == '[')
			break;

		const auto& line = lines[i];

		if (line.empty() || line[0] == '#' || line[0] == ';')
		{
			++i;
			continue;
		}

		const auto sep = line.find("=");
		if (sep == std::string::npos)
			ini_error(ini, i, "bad line '{}'", line);

		const std::string k = trim_copy(line.substr(0, sep));
		const std::string v = trim_copy(line.substr(sep + 1));

		if (k.empty())
			ini_error(ini, i, "bad line '{}'", line);

		if (section == "aliases")
		{
			add_alias(k, split_quoted(v, " "));
		}
		else if (task.empty())
		{
			if (add)
				details::add_string(section, k, v);
			else
				details::set_string(section, k, v);
		}
		else
		{
			if (task == "_override")
			{
				details::set_string_for_task("_override", section, k, v);
			}
			else
			{
				const auto& tasks = find_tasks(task);

				if (tasks.empty())
					ini_error(ini, i, "no task matching '{}' found", task);

				for (auto& t : tasks)
					details::set_string_for_task(t->name(), section, k, v);
			}
		}

		++i;
	}
}

void parse_ini(const fs::path& ini, bool add)
{
	gcx().debug(context::conf, "using ini at {}", ini);

	const auto lines = read_ini(ini);
	std::size_t i = 0;

	for (;;)
	{
		if (i >= lines.size())
			break;

		const auto& line = lines[i];
		if (line.empty() || line[0] == '#' || line[0] == ';')
		{
			++i;
			continue;
		}

		if (line.starts_with("[") && line.ends_with("]"))
		{
			const std::string s = line.substr(1, line.size() - 2);

			std::string task, section;

			const auto col = s.find(":");

			if (col == std::string::npos)
			{
				section = s;
			}
			else
			{
				task = s.substr(0, col);
				section = s.substr(col + 1);
			}

			parse_section(ini, i, lines, task, section, add);
		}
		else
		{
			ini_error(ini, i, "bad line '{}'", line);
		}
	}
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
			if (verbose)
				u8cout << "checking '" << i << "'\n";

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

		while (!cwd.empty()) {
			const auto in_cwd = cwd / default_ini_filename();
			if (fs::exists(in_cwd) && !fs::equivalent(in_cwd, master))
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

}	// namespace

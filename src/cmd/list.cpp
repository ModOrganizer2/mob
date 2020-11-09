#include "pch.h"
#include "commands.h"
#include "../tasks/tasks.h"

namespace mob
{

command::meta_t list_command::meta() const
{
	return
	{
		"list",
		"lists available tasks"
	};
}

clipp::group list_command::do_group()
{
	return clipp::group(
		clipp::command("list").set(picked_),

		(clipp::option("-h", "--help") >> help_)
			% "shows this message",

		(clipp::option("-a", "--all") >> all_)
			% "shows all the tasks, including pseudo parallel tasks",

		(clipp::option("-i", "--aliases") >> aliases_)
			% "shows only aliases",

		(clipp::opt_values(
			clipp::match::prefix_not("-"), "task", tasks_))
			% "with -a; when given, acts like the tasks given to `build` and "
			  "shows only the tasks that would run"
	);
}

int list_command::do_run()
{
	if (aliases_)
	{
		load_options();
		dump_aliases();
	}
	else
	{
		if (all_)
		{
			if (!tasks_.empty())
				set_task_enabled_flags(tasks_);

			load_options();
			dump(get_top_level_tasks(), 0);

			u8cout << "\n\naliases:\n";
			dump_aliases();
		}
		else
		{
			for (auto&& t : get_all_tasks())
				u8cout << " - " << join(t->names(), ", ") << "\n";
		}
	}


	return 0;
}

void list_command::dump(const std::vector<task*>& v, std::size_t indent) const
{
	for (auto&& t : v)
	{
		if (!t->enabled())
			continue;

		u8cout
			<< std::string(indent*4, ' ')
			<< " - " << join(t->names(), ",")
			<< "\n";

		if (auto* ct=dynamic_cast<container_task*>(t))
			dump(ct->children(), indent + 1);
	}
}

void list_command::dump_aliases() const
{
	const auto v = get_all_aliases();
	if (v.empty())
		return;

	for (auto&& [k, patterns] : v)
		u8cout << " - " << k << ": " << join(patterns, ", ") << "\n";
}


std::string list_command::do_doc()
{
	return
		"Lists all available tasks. The special task name `super` is a\n"
		"shorthand for all modorganizer tasks.";
}

}	// namespace

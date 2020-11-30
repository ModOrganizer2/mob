#pragma once

namespace mob
{

class task;

// thrown by tasks or within the task_manager when they're interrupted because
// of failure or sigint
//
class interrupted {};


// contains the tasks and aliases, singleton
//
// the manager owns the top level tasks added with add() but also has pointers
// to all tasks except for parallel_tasks, added by calling register_task() in
// task's constructor
//
class task_manager
{
public:
	// map of alias -> patterns
	using alias_map =
		std::map<std::string, std::vector<std::string>, std::less<>>;

	task_manager();
	static task_manager& instance();


	// adds a top-level task, used for running or interrupting tasks
	//
	void add(std::unique_ptr<task> t);

	// called by task::task() for all tasks except parallel_tasks, used for
	// find tasks by name
	//
	void register_task(task* t);


	// returns all tasks matching the glob
	//
	std::vector<task*> find(std::string_view pattern);

	// returns one task that matches the glob; returns null if multiple or no
	// tasks match the pattern, outputs errors when `verbose` is true (mostly
	// for the command line)
	//
	task* find_one(std::string_view pattern, bool verbose=true);

	// whether the given pattern matches at least one task or is "_override",
	// should only be used when parsing inis or command line options
	//
	bool valid_task_name(std::string_view pattern);


	// returns all tasks except for parallel_tasks
	//
	std::vector<task*> all();

	// returns all top-level tasks, that is, tasks added with add()
	//
	std::vector<task*> top_level();


	// adds an alias
	//
	void add_alias(std::string name, std::vector<std::string> patterns);

	// returns all aliases
	//
	const alias_map& aliases();


	// runs all top-level tasks sequentially, disabled tasks won't run
	//
	void run_all();

	// interrupts all tasks
	//
	void interrupt_all();

private:
	// top-level tasks
	std::vector<std::unique_ptr<task>> top_level_;

	// all tasks except for parallel_tasks
	std::vector<task*> all_;

	// set to true in interrupt_all(), checked in run_all() to stop the loop
	std::atomic<bool> interrupt_;

	// locked in interrupt_all() in case multiple tasks fail at the same time
	std::mutex interrupt_mutex_;

	// alias map
	alias_map aliases_;


	// used by find(), returns tasks matching the given glob
	//
	std::vector<task*> find_by_pattern(std::string_view pattern);

	// used by find(), looks for an alias with the given name and returns
	// matching tasks
	//
	std::vector<task*> find_by_alias(std::string_view alias_name);
};


// convenience, calls task_manager::add()
//
template <class Task, class... Args>
Task& add_task(Args&&... args)
{
	auto t = std::make_unique<Task>(std::forward<Args>(args)...);
	auto& ref = *t;

	task_manager::instance().add(std::move(t));

	return ref;
}

}	// namespace

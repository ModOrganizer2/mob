#pragma once

namespace mob
{

class interrupted {};
class task;

class task_manager
{
public:
	using alias_map =
		std::map<std::string, std::vector<std::string>, std::less<>>;


	task_manager();
	static task_manager& instance();

	void add(std::unique_ptr<task> t);

	std::vector<task*> find(std::string_view pattern);
	task* find_one(std::string_view pattern, bool verbose=true);
	bool valid_name(std::string_view pattern);

	std::vector<task*> all();
	std::vector<task*> top_level();

	void add_alias(std::string name, std::vector<std::string> patterns);
	const alias_map& aliases();

	void run_all();
	void interrupt_all();

	void register_task(task* t);

private:
	std::vector<std::unique_ptr<task>> top_level_;
	std::vector<task*> all_;
	std::atomic<bool> interrupt_;
	std::mutex interrupt_mutex_;
	alias_map aliases_;

	std::vector<task*> find_by_pattern(std::string_view pattern);
	std::vector<task*> find_by_alias(std::string_view pattern);
};

template <class Task, class... Args>
Task& add_task(Args&&... args)
{
	auto t = std::make_unique<Task>(std::forward<Args>(args)...);
	auto& ref = *t;

	task_manager::instance().add(std::move(t));

	return ref;
}

}	// namespace

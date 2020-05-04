#pragma once

#include "../utility.h"
#include "../context.h"

namespace mob
{

class task;

void add_task(std::unique_ptr<task> t);

template <class Task>
void add_task()
{
	add_task(std::make_unique<Task>());
}

bool run_task(const std::string& name);
void run_all_tasks();


class tool;

class task
{
public:
	task(const task&) = delete;
	task& operator=(const task&) = delete;

	virtual ~task();
	static void interrupt_all();

	const std::string& name() const;
	const std::vector<std::string>& names() const;

	virtual fs::path get_source_path() const = 0;

	void run();
	void interrupt();
	void join();

	void fetch();
	void build_and_install();
	void clean();

protected:
	context cx_;

	task(const char* name);
	task(std::vector<std::string> names);

	void check_interrupted();

	virtual void do_fetch() {}
	virtual void do_build_and_install() {}
	virtual void do_clean() {}

	template <class Tool>
	auto run_tool(Tool&& t)
	{
		{
			std::scoped_lock lock(tool_mutex_);
			tool_ = &t;
		}

		run_current_tool();

		{
			std::scoped_lock lock(tool_mutex_);
			tool_ = nullptr;
		}

		return t.result();
	}

private:
	std::vector<std::string> names_;
	std::thread thread_;
	std::atomic<bool> interrupted_;

	tool* tool_;
	std::mutex tool_mutex_;

	static std::mutex interrupt_mutex_;

	void run_current_tool();
};


template <class Task>
class basic_task : public task
{
public:
	using task::task;

	fs::path get_source_path() const override
	{
		return Task::source_path();
	}
};

}	// namespace

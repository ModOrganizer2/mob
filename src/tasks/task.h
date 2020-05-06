#pragma once

#include "../utility.h"
#include "../context.h"

namespace mob
{

class task;

void add_task(std::unique_ptr<task> t);

template <class Task, class... Args>
void add_task(Args&&... args)
{
	add_task(std::make_unique<Task>(std::forward<Args>(args)...));
}

void run_task(const std::string& name);
void run_tasks(const std::vector<std::string>& names);;
void run_all_tasks();
void list_tasks(bool err=false);


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
	virtual bool is_super() const;

	void run();
	void interrupt();
	void join();

	void fetch();
	void build_and_install();

protected:
	template <class... Names>
	task(std::string name, Names&&... names)
		: task(std::vector<std::string>{name, std::forward<Names>(names)...})
	{
	}

	task(std::vector<std::string> names);

	const context& cx() const;
	void add_name(std::string s);

	void check_interrupted();

	virtual void do_fetch() {}
	virtual void do_build_and_install() {}
	virtual void do_clean_for_rebuild() {}

	template <class Tool>
	auto run_tool(Tool&& t)
	{
		run_tool_impl(&t);
		return t.result();
	}

	void parallel(std::vector<std::pair<std::string, std::function<void ()>>> v)
	{
		std::vector<std::thread> ts;

		for (auto&& [name, f] : v)
		{
			cx().trace(context::generic, "running in parallel: " + name);

			ts.push_back(std::thread([this, name, f]
			{
				threaded_run(name, f);
			}));
		}

		for (auto&& t : ts)
			t.join();
	}

private:
	struct thread_context
	{
		std::thread::id tid;
		context cx;

		thread_context(std::thread::id tid, context cx)
			: tid(tid), cx(std::move(cx))
		{
		}
	};

	std::vector<std::string> names_;
	std::thread thread_;
	std::atomic<bool> interrupted_;

	std::vector<std::unique_ptr<thread_context>> contexts_;
	mutable std::mutex contexts_mutex_;

	std::vector<tool*> tools_;
	mutable std::mutex tools_mutex_;

	static std::mutex interrupt_mutex_;

	void clean_for_rebuild();
	void run_tool_impl(tool* t);
	void threaded_run(std::string name, std::function<void ()> f);
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

#pragma once

#include "../utility.h"
#include "../context.h"
#include "../tools/tools.h"

namespace mob
{

class task;
class tool;

void add_task(std::unique_ptr<task> t);

template <class Task, class... Args>
Task& add_task(Args&&... args)
{
	auto sp = std::make_unique<Task>(std::forward<Args>(args)...);
	auto* p = sp.get();
	add_task(std::move(sp));
	return *p;
}

void run_task(const std::string& name);
void run_tasks(const std::vector<std::string>& names);;
void run_all_tasks();
void list_tasks(bool err=false);


class task_conf_holder
{
public:
	task_conf_holder(std::vector<std::string> names)
		: names_(std::move(names))
	{
	}

	std::string mo_org()
	{
		return conf::option_by_name(names_, "mo_org");
	}

	std::string mo_branch()
	{
		return conf::option_by_name(names_, "mo_branch");
	}

	bool no_pull()
	{
		return conf::bool_option_by_name(names_, "no_pull");
	}

	git::ops git_op()
	{
		if (no_pull())
			return git::clone;
		else
			return git::clone_or_pull2;
	}

private:
	std::vector<std::string> names_;
};


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
	virtual std::string get_version() const = 0;
	virtual const bool get_prebuilt() const = 0;

	virtual bool is_super() const;

	virtual void run();
	virtual void interrupt();
	virtual void join();

	virtual void fetch();
	virtual void build_and_install();

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

	void threaded_run(std::string name, std::function<void ()> f);
	void parallel(std::vector<std::pair<std::string, std::function<void ()>>> v);

	task_conf_holder task_conf() const;

private:
	struct thread_context;

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

	std::string get_version() const override
	{
		return Task::version();
	}

	const bool get_prebuilt() const override
	{
		return Task::prebuilt();
	}
};


class parallel_tasks : public task
{
public:
	parallel_tasks(bool super);

	template <class Task, class... Args>
	parallel_tasks& add_task(Args&&... args)
	{
		children_.push_back(
			std::make_unique<Task>(std::forward<Args>(args)...));

		return *this;
	}

	fs::path get_source_path() const override
	{
		return {};
	}

	std::string get_version() const override
	{
		return {};
	}

	const bool get_prebuilt() const override
	{
		return false;
	}

	bool is_super() const override;

	void run() override;
	void interrupt() override;
	void join() override;

	void fetch() override;
	void build_and_install() override;

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	bool super_;
	std::vector<std::unique_ptr<task>> children_;
	std::vector<std::thread> threads_;
};

}	// namespace

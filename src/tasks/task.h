#pragma once

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

	virtual fs::path get_source_path() const = 0;

	void run();
	void interrupt();
	void join();

	void fetch();
	void build_and_install();
	void clean();

protected:
	task(std::string name);

	void check_interrupted();

	virtual void do_fetch() {}
	virtual void do_build_and_install() {}
	virtual void do_clean() {}

	template <class Tool>
	auto run_tool(Tool&& t)
	{
		{
			tool_ = &t;
			std::scoped_lock lock(tool_mutex_);
		}

		check_interrupted();
		t.run();
		check_interrupted();

		{
			std::scoped_lock lock(tool_mutex_);
			tool_ = nullptr;
		}

		return t.result();
	}

private:
	std::string name_;
	std::thread thread_;
	std::atomic<bool> interrupted_;

	tool* tool_;
	std::mutex tool_mutex_;

	static std::mutex interrupt_mutex_;
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

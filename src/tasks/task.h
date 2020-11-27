#pragma once

#include "../utility.h"
#include "../core/context.h"
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

void run_all_tasks();
bool is_super_task(const std::string& name);
std::vector<task*> find_tasks(std::string_view pattern);
task* find_one_task(std::string_view pattern, bool verbose=true);
bool valid_task_name(std::string_view pattern);

std::vector<task*> get_all_tasks();
std::vector<task*> get_top_level_tasks();

using alias_map = std::map<std::string, std::vector<std::string>, std::less<>>;
void add_alias(std::string name, std::vector<std::string> patterns);
const alias_map& get_all_aliases();


class task_conf_holder
{
public:
	task_conf_holder(const task& t);

	std::string mo_org() const;
	std::string mo_branch() const;
	bool no_pull() const;
	bool revert_ts() const;
	bool ignore_ts()const;
	std::string git_url_prefix() const;
	bool git_shallow() const;
	std::string git_user() const;
	std::string git_email() const;
	bool set_origin_remote() const;
	std::string remote_org() const;
	std::string remote_key() const;
	bool remote_no_push_upstream() const;
	bool remote_push_default_origin() const;

	git make_git(git::ops o=git::clone_or_pull) const;

	std::string make_git_url(
		const std::string& org, const std::string& repo) const;

private:
	const task& task_;
};


class task
{
public:
	enum class clean
	{
		nothing     = 0x00,
		redownload  = 0x01,
		reextract   = 0x02,
		reclone     = reextract,
		reconfigure = 0x04,
		rebuild     = 0x08,
		everything  = redownload+reextract+reconfigure+rebuild
	};

	task(const task&) = delete;
	task& operator=(const task&) = delete;
	virtual ~task();

	static void interrupt_all();

	virtual bool enabled() const;
	const std::string& name() const;
	const std::vector<std::string>& names() const;

	virtual fs::path get_source_path() const = 0;
	virtual std::string get_version() const = 0;
	virtual const bool get_prebuilt() const = 0;

	virtual bool is_super() const;

	virtual void run();
	virtual void interrupt();
	virtual void join();

	virtual void clean_task();
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

	virtual void do_clean(clean) {};
	virtual void do_fetch() {}
	virtual void do_build_and_install() {}

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

	clean make_clean_flags() const;
	void run_tool_impl(tool* t);
};


MOB_ENUM_OPERATORS(task::clean);


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


class container_task : public task
{
public:
	using task::task;

	virtual std::vector<task*> children() const = 0;
};


class parallel_tasks : public container_task
{
public:
	parallel_tasks();

	bool enabled() const override
	{
		return true;
	}

	template <class Task, class... Args>
	parallel_tasks& add_task(Args&&... args)
	{
		add_task(std::make_unique<Task>(std::forward<Args>(args)...));
		return *this;
	}

	template <class Task, class T, class... Args>
	parallel_tasks& add_task(std::initializer_list<T> il, Args&&... args)
	{
		add_task(std::make_unique<Task>(
			std::move(il), std::forward<Args>(args)...));

		return *this;
	}

	void add_task(std::unique_ptr<task> t);

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

	std::vector<task*> children() const override;

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean(clean c) override;

private:
	std::vector<std::unique_ptr<task>> children_;
	std::vector<std::thread> threads_;
};

}	// namespace

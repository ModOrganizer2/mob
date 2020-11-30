#pragma once

#include "../utility.h"

namespace mob
{

class tool;
class conf_task;
class git;


// base class for all tasks, although tasks will actually inherit from
// basic_task<> below
//
class task
{
public:
	// passed to do_clean(), can be any combination depending on the
	// configuration (redownload, reextract, etc. in the ini)
	//
	enum class clean
	{
		// do_clean() never gets that
		nothing     = 0x00,

		// whatever downloaded file that was cached must be deleted so it can
		// be downloaded again; cached downloads normally inhibit downloads
		redownload  = 0x01,

		// extracting or cloning must be fresh, so basically delete the whole
		// source directory; both enums have the same value, but `reclone` makes
		// more sense for tools that don't actually extract archives
		reextract   = 0x02,
		reclone     = reextract,

		// building the task must run whatever configuration tool from scratch,
		// such as deleting the vsbuild directory for cmake tasks
		reconfigure = 0x04,

		// the task must be rebuilt from scratch, but without reconfiguration,
		// such as `msbuild` with the `Clean` target
		//
		// some tasks don't have an equivalent for this and might just delete
		// the whole source directory, such as openssl
		rebuild     = 0x08
	};


	task(const task&) = delete;
	task& operator=(const task&) = delete;

	// anchor
	//
	virtual ~task();


	// whether this task is enabled, just checks conf().task(), but
	// parallel_tasks overrides this below to always be true
	//
	virtual bool enabled() const;

	// main task name
	//
	const std::string& name() const;

	// all names for this task
	//
	const std::vector<std::string>& names() const;

	// case insensitive, underscores and dashes are equivalent; gets converted
	// to a regex where * becomes .*
	//
	bool name_matches(std::string_view pattern) const;


	// path to the source directory, something like prefix/build/7zip-xx or
	// or prefix/build/modorganizer_super/uibase
	//
	// used for auto patching in fetch(), returns an empty path here
	//
	virtual fs::path get_source_path() const;

	// whether this task should use the prebuilt version
	//
	// used for auto patching in fetch(), returns false here
	//
	virtual bool get_prebuilt() const;


	// if the task is enabled, calls fetch() and build_and_install()
	//
	virtual void run();

	// sets the interrupt flag on this task so it's picked up in run() and
	// calls interrupt() on all tools currently running
	//
	virtual void interrupt();

protected:
	using parallel_functions =
		std::vector<std::pair<std::string, std::function<void ()>>>;

	template <class... Names>
	task(std::string name, Names&&... names)
		: task(std::vector<std::string>{name, std::forward<Names>(names)...})
	{
	}

	task(std::vector<std::string> names);


	// implemented by derived classes to clean the task depending on the given
	// clean flags; no-op in this class
	//
	virtual void do_clean(clean);

	// implemented by derived classes to fetch (download, clone, etc.) the
	// required files to build and/or install the task; no-op in this class
	//
	virtual void do_fetch();

	// implemented by derived classes to build and install the task in one step;
	// no-op in this class
	//
	virtual void do_build_and_install();


	// returns the task's context
	//
	// since a task may be running several threads, a list of per-thread context
	// objects is kept in contexts_ and the correct one is returned
	//
	const context& cx() const;

	// throws if the interrupted flag is set
	//
	void check_interrupted();

	// adds the tool to the internal list of active tools so they can be
	// interrupted properly and calls run() on it with the task's log context
	//
	// once run() returns, removes the tool from the list and returns whatever
	// result() returns, which may be void
	//
	template <class Tool>
	auto run_tool(Tool&& t)
	{
		run_tool_impl(&t);
		return t.result();
	}

	// runs the given functions in a thread_pool with `threads` as the maximum
	// number of threads
	//
	// calls threaded_run() for every function, which creates a new log context
	// for the thread in case multiple tools are run simultaneously
	//
	// this is the preferred way for tasks to run tools in parallel, such as
	// in the translations or gtest tasks
	//
	void parallel(parallel_functions v, std::optional<std::size_t> threads={});

	// returns the conf_task for this task, short for conf().task(names())
	//
	conf_task task_conf() const;

	// returns a git tool suitable for this task, with all the relevant task
	// settings set, such as ignore_ts_on_clone, shallow, etc.
	//
	// if `o` is clone_or_pull and the configuration has no_pull for this task,
	// it is changed to clone only
	//
	git make_git() const;

	// returns a git url for the given org and repo, using the git_url_prefix
	// for this task
	//
	std::string make_git_url(
		const std::string& org, const std::string& repo) const;

private:
	// a struct with the thread id and a context object, defined in task.cpp
	// to avoid pulling to many includes
	//
	struct thread_context;

	// names for this task
	const std::vector<std::string> names_;

	// set when interrupt() is called, checked by check_interrupted(), which
	// throws an `interrupted` exception
	//
	std::atomic<bool> interrupted_;

	// holds a context per thread, added/removed in threaded_run()
	std::vector<std::unique_ptr<thread_context>> contexts_;
	mutable std::mutex contexts_mutex_;

	// list of active tools, added/removed in run_tool()
	std::vector<tool*> tools_;
	mutable std::mutex tools_mutex_;


	// called by run_tool, does the actual work
	//
	void run_tool_impl(tool* t);

	// called by name_matches() when the pattern is a glob
	//
	bool name_matches_glob(std::string_view pattern) const;

	// called by name_matches() when the pattern is not a glob
	//
	bool name_matches_string(std::string_view pattern) const;

	// called by name_matches_string(), compares the two strings to get the
	// same result as name_matches_glob() (case insensitive, dashes/underscores
	// are the same, etc.) but without requiring a regex because it's slow as
	// frick
	//
	bool strings_match(std::string_view a, std::string_view b) const;

	// called by run() and parallel(), adds a new context for the current thread
	// and calls f()
	//
	void threaded_run(std::string name, std::function<void ()> f);


	// calls clean_task(), then do_fetch() if needed (see --no-fetch-task);
	// no-op if the task is disabled
	//
	void fetch();

	// calls do_build_and_install() if building is enabled
	// (see --no-build-task); no-op if the task is disabled
	//
	void build_and_install();

	// calls do_clean() if needed with the appropriate flags (see
	// --no-clean-task); no-op if the task is disabled
	//
	void clean_task();

};


MOB_ENUM_OPERATORS(task::clean);


// all tasks except for modorganizer have static functions source_path() and
// prebuilt(), which are used in a variety of places, but `task` also needs to
// know about them
//
// so get_source_path() and get_prebuilt() just forward to the static version
// and basic_task uses CRTP so tasks don't have to implement both
//
// since the modorganizer task is reused for all super projects, it doesn't
// have the static member functions and implement these two functions itself
//
template <class Task>
class basic_task : public task
{
public:
	using task::task;

	fs::path get_source_path() const override
	{
		return Task::source_path();
	}

	bool get_prebuilt() const override
	{
		return Task::prebuilt();
	}
};


// a task that overrides run() to start as many threads as it has children
// and calls run() on all of them
//
class parallel_tasks : public task
{
public:
	parallel_tasks();

	// joins
	//
	~parallel_tasks();


	// creates a task `Task`, forwards args to constructor and adds it
	//
	template <class Task, class... Args>
	parallel_tasks& add_task(Args&&... args)
	{
		add_task(std::make_unique<Task>(std::forward<Args>(args)...));
		return *this;
	}

	// creates a task `Task`, forwards args to constructor and adds it
	//
	// this overload is convenient for modorganizer tasks to pass the task names
	// as an initializer list, which can't be done with the version above
	// because `Args` can't be deduced
	//
	template <class Task, class T, class... Args>
	parallel_tasks& add_task(std::initializer_list<T> il, Args&&... args)
	{
		add_task(std::make_unique<Task>(
			std::move(il), std::forward<Args>(args)...));

		return *this;
	}

	// called by the above, adds the task
	//
	void add_task(std::unique_ptr<task> t);


	// returns true, parallel tasks cannot be disabled, but their children can
	//
	bool enabled() const override;

	// starts a thread for every child task and calls run() on it
	//
	void run() override;

	// calls interrupt() on all children tasks
	//
	void interrupt() override;

	// returns children tasks
	//
	std::vector<task*> children() const;

private:
	// tasks
	std::vector<std::unique_ptr<task>> children_;

	// one thread per task
	std::vector<std::thread> threads_;


	// joins all threads
	//
	void join();
};

}	// namespace

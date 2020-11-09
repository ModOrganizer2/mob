#pragma once

namespace mob
{

// sets unhandled exception and std::terminate handlers for the current thread
//
void set_thread_exception_handlers();


// starts a thread with exception handlers set up
//
template <class F>
std::thread start_thread(F&& f)
{
	return std::thread([f]
	{
		set_thread_exception_handlers();
		f();
	});
}


// executes a function in a thread, blocks if there are too many
//
class thread_pool
{
public:
	typedef std::function<void ()> fun;

	thread_pool(std::size_t count=std::thread::hardware_concurrency());
	~thread_pool();

	// non-copyable
	thread_pool(const thread_pool&) = delete;
	thread_pool& operator=(const thread_pool&) = delete;

	// runs the given function in a thread; if there no threads available,
	// blocks until another thread finishes
	//
	void add(fun f);

	// blocks until all threads are finished
	//
	void join();

private:
	struct thread_info
	{
		std::atomic<bool> running = false;
		fun thread_fun;
		std::thread thread;
	};

	const std::size_t count_;
	std::vector<std::unique_ptr<thread_info>> threads_;

	// tries to find an available thread, returns false if none are found
	//
	bool try_add(fun thread_fun);
};

}	// namespace

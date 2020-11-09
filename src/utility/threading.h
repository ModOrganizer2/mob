#pragma once

namespace mob
{

void set_thread_exception_handlers();

template <class F>
std::thread start_thread(F&& f)
{
	return std::thread([f]
	{
		set_thread_exception_handlers();
		f();
	});
}


class thread_pool
{
public:
	typedef std::function<void ()> fun;

	thread_pool(std::size_t count=std::thread::hardware_concurrency());
	~thread_pool();

	// non-copyable
	thread_pool(const thread_pool&) = delete;
	thread_pool& operator=(const thread_pool&) = delete;

	void add(fun f);
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

	bool try_add(fun thread_fun);
};

}	// namespace

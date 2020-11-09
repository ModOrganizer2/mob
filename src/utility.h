#pragma once

#include "utility/algo.h"
#include "utility/fs.h"
#include "utility/io.h"
#include "utility/string.h"

namespace mob
{

// enum stuff
#define MOB_ENUM_OPERATORS(E) \
	inline E operator|(E e1, E e2) { return (E)((int)e1 | (int)e2); } \
	inline E operator&(E e1, E e2) { return (E)((int)e1 & (int)e2); } \
	inline E operator|=(E& e1, E e2) { e1 = e1 | e2; return e1; }

template <class E>
bool is_set(E e, E v)
{
	static_assert(std::is_enum_v<E>, "this is for enums");
	return (e & v) == v;
}

template <class E>
bool is_any_set(E e, E v)
{
	static_assert(std::is_enum_v<E>, "this is for enums");
	return (e & v) != E(0);
}


// time since mob started
//
std::chrono::nanoseconds timestamp();


// exception thrown when a task failed
//
class bailed
{
public:
	bailed(std::string s={})
		: s_(std::move(s))
	{
	}

	const char* what() const
	{
		return s_.c_str();
	}

private:
	std::string s_;
};


// executes the given function in the destructor
//
template <class F>
class guard
{
public:
	guard(F f)
		: f_(f)
	{
	}

	~guard()
	{
		f_();
	}

private:
	F f_;
};


// used to keep track of how long things have taken; instruments N things, each
// thing has a name and a list of start/end time
//
// example:
//
//    enum class tasks
//    {
//       one_thing, another_thing
//    };
//
//     instrumentable<2> i("test", {"one", "two"});
//
//     i.instrument<tasks::one_thing>([]{ do_one(); });  // takes 1s
//     i.instrument<tasks::one_thing>([]{ do_two(); });  // takes 1s
//     i.instrument<tasks::one_thing>([]{ do_one(); });  // takes 2s
//     i.instrument<tasks::one_thing>([]{ do_two(); });  // takes 2s
//
// at this point, `i.instrumented_tasks()` has this, assuming a start time of 0:
//
// { "one", {0, 1}, {3, 5} }
// { "two", {1, 2}, {5, 7} }
//
// build_command::dump_timings() uses this to generate a text file with the
// timings
//
template <std::size_t N>
class instrumentable
{
public:
	// start and end times
	//
	struct time_pair
	{
		std::chrono::nanoseconds start{}, end{};
	};

	// a thing to time
	//
	struct task
	{
		std::string name;
		std::vector<time_pair> tps;
	};

	// sets the end time of the given time_pair in the destructor
	//
	struct timing_ender
	{
		timing_ender(time_pair& tp)
			: tp(tp)
		{
		}

		~timing_ender()
		{
			tp.end = timestamp();
		}

		time_pair& tp;
	};


	// an instrumentable has name and so do all of its tasks
	//
	instrumentable(std::string name, std::array<std::string, N> names)
		: name_(std::move(name))
	{
		for (std::size_t i=0; i<N; ++i)
			tasks_[i].name = names[i];
	}

	const std::string& instrumentable_name() const
	{
		return name_;
	}

	// adds a new timing to task `E` (typically an enum); the start time is set
	// to now, f() is executed, and the end time is set to now
	//
	template <auto E, class F>
	auto instrument(F&& f)
	{
		auto& t = std::get<static_cast<std::size_t>(E)>(tasks_);

		// add new timing
		t.tps.push_back({});
		t.tps.back().start = timestamp();

		// will set end the time
		timing_ender te(t.tps.back());

		return f();
	}

	const std::array<task, N>& instrumented_tasks() const
	{
		return tasks_;
	}

private:
	std::string name_;
	std::array<task, N> tasks_;
};


enum class arch
{
	x86 = 1,
	x64,
	dont_care,

	def = x64
};


class url;

// returns a url for a prebuilt binary having the given filename; prebuilts are
// hosted on github, in the umbrella repo
//
url make_prebuilt_url(const std::string& filename);

// returns a url for an appveyor artifact; this is used by usvfs for prebuilts
//
url make_appveyor_artifact_url(
	arch a, const std::string& project, const std::string& filename);

}	// namespace

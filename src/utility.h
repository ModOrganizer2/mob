#pragma once

#include "utility/algo.h"
#include "utility/fs.h"
#include "utility/io.h"
#include "utility/string.h"

namespace mob
{

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


class context;
class url;

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


// time since mob started
//
std::chrono::nanoseconds timestamp();


template <std::size_t N>
class instrumentable
{
public:
	struct time_pair
	{
		std::chrono::nanoseconds start{}, end{};
	};

	struct task
	{
		std::string name;
		std::vector<time_pair> tps;
	};

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

	template <auto E, class F>
	auto instrument(F&& f)
	{
		auto& t = std::get<static_cast<std::size_t>(E)>(tasks_);
		t.tps.push_back({});
		t.tps.back().start = timestamp();
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


enum class arch
{
	x86 = 1,
	x64,
	dont_care,

	def = x64
};


url make_prebuilt_url(const std::string& filename);
url make_appveyor_artifact_url(
	arch a, const std::string& project, const std::string& filename);

}	// namespace

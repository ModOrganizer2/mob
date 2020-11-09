#pragma once

#include "utility/string.h"
#include "utility/fs.h"

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




class console_color
{
public:
	enum colors
	{
		white,
		grey,
		yellow,
		red
	};

	console_color();
	console_color(colors c);
	~console_color();

private:
	bool reset_;
	WORD old_atts_;
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


class u8stream
{
public:
	u8stream(bool err)
		: err_(err)
	{
	}

	template <class... Args>
	u8stream& operator<<(Args&&... args)
	{
		std::ostringstream oss;
		((oss << std::forward<Args>(args)), ...);

		do_output(oss.str());

		return *this;
	}

	void write_ln(std::string_view utf8);

private:
	bool err_;

	void do_output(const std::string& s);
};


extern u8stream u8cout;
extern u8stream u8cerr;

void set_std_streams();
std::mutex& global_output_mutex();



template <class T>
class repeat_iterator
{
public:
	repeat_iterator(const T& s)
		: s_(s)
	{
	}

	bool operator==(const repeat_iterator&) const
	{
		return false;
	}

	const T& operator*() const
	{
		return s_;
	}

	repeat_iterator& operator++()
	{
		// no-op
		return *this;
	}

private:
	const T& s_;
};


template <class T>
class repeat_range
{
public:
	using value_type = T;
	using const_iterator = repeat_iterator<value_type>;

	repeat_range(const T& s)
		: s_(s)
	{
	}

	const_iterator begin() const
	{
		return const_iterator(s_);
	}

	const_iterator end() const
	{
		return const_iterator(s_);
	}

private:
	T s_;
};


template <class T>
repeat_iterator<T> begin(const repeat_range<T>& r)
{
	return r.begin();
}

template <class T>
repeat_iterator<T> end(const repeat_range<T>& r)
{
	return r.end();
}


template <class T>
struct repeat_converter
{
	using value_type = T;
};

template <>
struct repeat_converter<const char*>
{
	using value_type = std::string;
};

template <std::size_t N>
struct repeat_converter<const char[N]>
{
	using value_type = std::string;
};

template <std::size_t N>
struct repeat_converter<char[N]>
{
	using value_type = std::string;
};


template <class T>
auto repeat(const T& s)
{
	return repeat_range<typename repeat_converter<T>::value_type>(s);
}


template <
	class Range1,
	class Range2,
	class Container=std::vector<std::pair<
	typename Range1::value_type,
	typename Range2::value_type>>>
Container zip(const Range1& range1, const Range2& range2)
{
	Container out;

	auto itor1 = begin(range1);
	auto end1 = end(range1);

	auto itor2 = begin(range2);
	auto end2 = end(range2);

	for (;;)
	{
		if (itor1 == end1 || itor2 == end2)
			break;

		out.push_back({*itor1, *itor2});
		++itor1;
		++itor2;
	}

	return out;
}


template <class F, class T>
auto map(const std::vector<T>& v, F&& f)
{
	using mapped_type = decltype(f(std::declval<T>()));
	std::vector<mapped_type> out;

	for (auto&& e : v)
		out.push_back(f(e));

	return out;
}


// see https://github.com/isanae/mob/issues/4
//
// this restores the original console font if it changed
//
class font_restorer
{
public:
	font_restorer()
		: restore_(false)
	{
		std::memset(&old_, 0, sizeof(old_));
		old_.cbSize = sizeof(old_);

		if (GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &old_))
			restore_ = true;
	}

	~font_restorer()
	{
		if (!restore_)
			return;

		CONSOLE_FONT_INFOEX now = {};
		now.cbSize = sizeof(now);

		if (!GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &now))
			return;

		if (std::wcsncmp(old_.FaceName, now.FaceName, LF_FACESIZE) != 0)
			restore();
	}

	void restore()
	{
		::SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &old_);
	}

private:
	CONSOLE_FONT_INFOEX old_;
	bool restore_;
};

}	// namespace

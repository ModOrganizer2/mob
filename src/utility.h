#pragma once

namespace mob
{

#define MOB_ENUM_OPERATORS(E) \
	inline E operator|(E e1, E e2) { return (E)((int)e1 | (int)e2); } \
	inline E operator|=(E& e1, E e2) { e1 = e1 | e2; return e1; }


class context;

class bailed
{
public:
	bailed(std::string s)
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


struct handle_closer
{
	using pointer = HANDLE;

	void operator()(HANDLE h)
	{
		if (h != INVALID_HANDLE_VALUE)
			::CloseHandle(h);
	}
};

using handle_ptr = std::unique_ptr<HANDLE, handle_closer>;


struct file_closer
{
	void operator()(std::FILE* f)
	{
		if (f)
			std::fclose(f);
	}
};

using file_ptr = std::unique_ptr<FILE, file_closer>;


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


class file_deleter
{
public:
	file_deleter(const context& cx, fs::path p);
	file_deleter(const file_deleter&) = delete;
	file_deleter& operator=(const file_deleter&) = delete;
	~file_deleter();

	void delete_now();
	void cancel();

private:
	const context& cx_;
	fs::path p_;
	bool delete_;
};


class directory_deleter
{
public:
	directory_deleter(const context& cx, fs::path p);
	directory_deleter(const directory_deleter&) = delete;
	directory_deleter& operator=(const directory_deleter&) = delete;
	~directory_deleter();

	void delete_now();
	void cancel();

private:
	const context& cx_;
	fs::path p_;
	bool delete_;
};


class interruption_file
{
public:
	interruption_file(const context& cx, fs::path dir, std::string name);

	fs::path file() const;
	bool exists() const;

	void create();
	void remove();

private:
	const context& cx_;
	fs::path dir_;
	std::string name_;
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


std::string replace_all(
	std::string s, const std::string& from, const std::string& to);

std::string join(const std::vector<std::string>& v, const std::string& sep);

std::string pad_right(std::string s, std::size_t n, char c=' ');
std::string pad_left(std::string s, std::size_t n, char c=' ');

template <class F>
void for_each_line(std::string_view s, F&& f)
{
	const char* start = s.data();
	const char* end = s.data() + s.size();
	const char* p = start;

	for (;;)
	{
		if (p == end || *p == '\n' || *p == '\r')
		{
			if (p != start)
				f(std::string_view(start, static_cast<std::size_t>(p - start)));

			while (p != end && (*p == '\n' || *p == '\r'))
				++p;

			if (p == end)
				break;

			start = p;
		}
		else
		{
			++p;
		}
	}
}


enum class arch
{
	x86 = 1,
	x64,
	dont_care,

	def = x64
};


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


template <class F>
std::vector<std::string> map(const std::vector<std::string>& v, F&& f)
{
	std::vector<std::string> out;

	for (auto&& e : v)
		out.push_back(f(e));

	return out;
}

}	// namespace

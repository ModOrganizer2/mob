#pragma once

namespace mob
{

class url;

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


class file_deleter
{
public:
	file_deleter(fs::path p);
	file_deleter(const file_deleter&) = delete;
	file_deleter& operator=(const file_deleter&) = delete;
	~file_deleter();

	void delete_now();
	void cancel();

private:
	fs::path p_;
	bool delete_;
};


class directory_deleter
{
public:
	directory_deleter(fs::path p);
	directory_deleter(const directory_deleter&) = delete;
	directory_deleter& operator=(const directory_deleter&) = delete;
	~directory_deleter();

	void delete_now();
	void cancel();

private:
	fs::path p_;
	bool delete_;
};


enum class level
{
	debug, info, warning, error, bail
};


std::string error_message(DWORD e);

void out(level lv, const std::string& s);
void out(level lv, const std::string& s, DWORD e);
void out(level lv, const std::string& s, const std::error_code& e);
void dump_logs();

template <class... Args>
[[noreturn]] void bail_out(Args&&... args)
{
	out(level::bail, std::forward<Args>(args)...);
}

template <class... Args>
void error(Args&&... args)
{
	out(level::error, std::forward<Args>(args)...);
}

template <class... Args>
void warn(Args&&... args)
{
	out(level::warning, std::forward<Args>(args)...);
}

template <class... Args>
void info(Args&&... args)
{
	out(level::info, std::forward<Args>(args)...);
}

template <class... Args>
void debug(Args&&... args)
{
	out(level::debug, std::forward<Args>(args)...);
}


std::string read_text_file(const fs::path& p);

std::string replace_all(
	std::string s, const std::string& from, const std::string& to);

std::string join(const std::vector<std::string>& v, const std::string& sep);


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

}	// namespace

#pragma once

namespace mob
{

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


// a range that infinitely return `s`, can be used with zip() to create a pair
// with a repeating value
//
template <class T>
auto repeat(const T& s)
{
	return repeat_range<typename repeat_converter<T>::value_type>(s);
}


// returns a container of pairs from both ranges; if the ranges are not the
// same size, truncates to the smallest one
//
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

// returns a vector containing the result of `f(e)` for each element `e` of `v`
//
template <class F, class T>
auto map(const std::vector<T>& v, F&& f)
{
	using mapped_type = decltype(f(std::declval<T>()));
	std::vector<mapped_type> out;

	for (auto&& e : v)
		out.push_back(f(e));

	return out;
}

}	// namespace

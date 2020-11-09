#pragma once

#include "assert.h"

namespace mob
{

enum class encodings
{
	dont_know = 0,
	utf8,
	utf16,
	acp,
	oem
};


// case insensitive, underscores and dashes are equivalent; gets converted to
// a regex where * becomes .*
//
bool glob_match(const std::string& pattern, const std::string& s);

std::string replace_all(
	std::string s, const std::string& from, const std::string& to);

template <class T, class Sep>
T join(const std::vector<T>& v, const Sep& sep)
{
	T s;
	bool first = true;

	for (auto&& e : v)
	{
		if (!first)
			s += sep;

		s += e;
		first = false;
	}

	return s;
}

std::vector<std::string> split(const std::string& s, const std::string& seps);
std::vector<std::string> split_quoted(const std::string& s, const std::string& seps);

std::string pad_right(std::string s, std::size_t n, char c=' ');
std::string pad_left(std::string s, std::size_t n, char c=' ');

void trim(std::string& s, std::string_view what=" \t\r\n");
void trim(std::wstring& s, std::wstring_view what=L" \t\r\n");

std::string table(
	const std::vector<std::pair<std::string, std::string>>& v,
	std::size_t indent, std::size_t spacing);

std::string trim_copy(std::string_view s, std::string_view what=" \t\r\n");
std::wstring trim_copy(std::wstring_view s, std::wstring_view what=L" \t\r\n");

std::wstring utf8_to_utf16(std::string_view s);
std::string utf16_to_utf8(std::wstring_view ws);
std::string bytes_to_utf8(encodings e, std::string_view bytes);
std::string utf8_to_bytes(encodings e, std::string_view utf8);

template <class T>
std::string path_to_utf8(T&&) = delete;

std::string path_to_utf8(fs::path p);


template <class F>
void for_each_line(std::string_view s, F&& f)
{
	if (s.empty())
		return;

	const char* const begin = s.data();
	const char* const end = s.data() + s.size();

	const char* start = begin;
	const char* p = begin;

	for (;;)
	{
		MOB_ASSERT(p && p >= begin && p <= end);
		MOB_ASSERT(start && start >= begin && start <= end);

		if (p == end || *p == '\n' || *p == '\r')
		{
			if (p != start)
			{
				MOB_ASSERT(p >= start);

				const auto n = static_cast<std::size_t>(p - start);
				MOB_ASSERT(n <= s.size());

				f(std::string_view(start, n));
			}

			while (p != end && (*p == '\n' || *p == '\r'))
				++p;

			MOB_ASSERT(p && p >= begin && p <= end);

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

}	// namespace

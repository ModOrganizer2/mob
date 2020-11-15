#pragma once

#include "assert.h"

namespace mob
{

enum class encodings
{
	dont_know = 0,
	utf8,
	utf16,
	acp,  // active code page
	oem   // oem code page
};


// case insensitive, underscores and dashes are equivalent; gets converted to
// a regex where * becomes .*
//
bool glob_match(const std::string& pattern, const std::string& s);

// replaces all instances of `from` by `to`, returns a copy
//
std::string replace_all(
	std::string s, const std::string& from, const std::string& to);

// concatenates all elements of `v`, separated by `sep`
//
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

// splits the given string on any character in `seps`
//
std::vector<std::string> split(const std::string& s, const std::string& seps);

// splits the given string on any character in `seps` that are not between
// double quotes
//
std::vector<std::string> split_quoted(
  const std::string& s, const std::string& seps);


// adds enough of character `c` to the end of string `s` so its length is `n`
// character
//
std::string pad_right(std::string s, std::size_t n, char c=' ');

// adds enough of character `c` to the start of string `s` so its length is `n`
// character
//
std::string pad_left(std::string s, std::size_t n, char c=' ');


// removes any character found in `what` from both start and end of string;
// in-place
//
void trim(std::string& s, std::string_view what=" \t\r\n");
void trim(std::wstring& s, std::wstring_view what=L" \t\r\n");


// removes any character found in `what` from both start and end of string;
// returns a copy
//
std::string trim_copy(std::string_view s, std::string_view what=" \t\r\n");
std::wstring trim_copy(std::wstring_view s, std::wstring_view what=L" \t\r\n");

// formats a vector of pairs into two columns, putting `indent` spaces at the
// start of each line and `spacing` spaces between the columns
//
std::string table(
	const std::vector<std::pair<std::string, std::string>>& v,
	std::size_t indent, std::size_t spacing);

// converts a utf8 string to utf16
//
std::wstring utf8_to_utf16(std::string_view s);

// converts a utf16 string to utf8
//
std::string utf16_to_utf8(std::wstring_view ws);

// converts bytes of the given encoding to utf8
//
std::string bytes_to_utf8(encodings e, std::string_view bytes);

// converts a utf8 string to bytes of the given encoding
//
std::string utf8_to_bytes(encodings e, std::string_view utf8);


// don't allow this for anything else than an fs::path
template <class T>
std::string path_to_utf8(T&&) = delete;

// convert a path to utf8; p.u8string() would work, but it returns a
// std::u8string instead of an std::string
//
std::string path_to_utf8(fs::path p);


// calls f() for each line in the given string, skipping empty lines
//
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
			// end of line or string

			if (p != start)
			{
				// line was not empty
				MOB_ASSERT(p >= start);

				const auto n = static_cast<std::size_t>(p - start);
				MOB_ASSERT(n <= s.size());

				f(std::string_view(start, n));
			}

			// skip to start of next line
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


class encoded_buffer
{
public:
	encoded_buffer(encodings e=encodings::dont_know, std::string bytes={});

	void add(std::string_view bytes);

	std::string utf8_string() const;

	template <class F>
	void next_utf8_lines(bool finished, F&& f)
	{
		for (;;)
		{
			switch (e_)
			{
				case encodings::utf16:
				{
					std::wstring_view utf16 =
						next_line<wchar_t>(finished, bytes_, last_);

					if (utf16.empty())
						return;

					f(utf16_to_utf8(utf16));
					break;
				}

				case encodings::acp:
				case encodings::oem:
				{
					std::string_view cp =
						next_line<char>(finished, bytes_, last_);

					if (cp.empty())
						return;

					f(bytes_to_utf8(e_, cp));
					break;
				}

				case encodings::utf8:
				case encodings::dont_know:
				default:
				{
					std::string_view utf8 =
						next_line<char>(finished, bytes_, last_);

					if (utf8.empty())
						return;

					f(std::string(utf8));
					break;
				}
			}
		}
	}

private:
	encodings e_;
	std::string bytes_;
	std::size_t last_;

	template <class CharT>
	std::basic_string_view<CharT> next_line(
		bool finished, std::string_view bytes, std::size_t& byte_offset)
	{
		std::size_t size = bytes.size();

		if constexpr (sizeof(CharT) == 2)
		{
			if ((size & 1) == 1)
				--size;
		}

		const CharT* start = reinterpret_cast<const CharT*>(bytes.data() + byte_offset);
		const CharT* end = reinterpret_cast<const CharT*>(bytes.data() + size);
		const CharT* p = start;

		std::basic_string_view<CharT> line;

		while (p != end)
		{
			if (*p == CharT('\n') || *p == CharT('\r'))
			{
				line = {start, static_cast<std::size_t>(p - start)};

				while (p != end && (*p == CharT('\n') || *p == CharT('\r')))
					++p;

				if (!line.empty())
					break;

				start = p;
			}
			else
			{
				++p;
			}
		}

		if (line.empty())
		{
			if (finished)
			{
				line = {
					reinterpret_cast<const CharT*>(bytes.data() + byte_offset),
					size - byte_offset
				};

				byte_offset = bytes.size();
			}
		}
		else
		{
			byte_offset = static_cast<std::size_t>(
				reinterpret_cast<const char*>(p) - bytes.data());

			MOB_ASSERT(byte_offset <= bytes.size());
		}

		return line;
	}
};

}	// namespace

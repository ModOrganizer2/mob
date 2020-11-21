#include "pch.h"
#include "string.h"
#include "../utility.h"

namespace mob
{

bool glob_match(std::string_view pattern, std::string_view s)
{
	try
	{
		std::string fixed_pattern(pattern);
		fixed_pattern = replace_all(fixed_pattern, "*", ".*");
		fixed_pattern = replace_all(fixed_pattern, "_", "-");

		std::string fixed_string(s);
		fixed_string = replace_all(fixed_string, "_", "-");

		std::regex re(fixed_pattern, std::regex::icase);

		return std::regex_match(fixed_string, re);
	}
	catch(std::exception&)
	{
		u8cerr
			<< "bad glob '" << pattern << "'\n"
			<< "globs are actually bastardized regexes where '*' is "
			<< "replaced by '.*', so don't push it\n";

		throw bailed();
	}
}

std::string replace_all(
	std::string s, const std::string& from, const std::string& to)
{
	std::size_t start = 0;

	for (;;)
	{
		const auto pos = s.find(from, start);
		if (pos == std::string::npos)
			break;

		s.replace(pos, from.size(), to);
		start = pos + to.size();
	}

	return s;
}

std::vector<std::string> split(const std::string& s, const std::string& seps)
{
	std::vector<std::string> v;

	std::size_t start = 0;

	while (start < s.size())
	{
		auto p = s.find_first_of(seps, start);
		if (p == std::string::npos)
			p = s.size();

		if (p - start > 0)
			v.push_back(s.substr(start, p - start));

		start = p + 1;
	}

	return v;
}

std::vector<std::string> split_quoted(const std::string& s, const std::string& seps)
{
	std::vector<std::string> v;
	std::string token;

	// currently between double quotes
	bool q = false;

	for (std::size_t i=0; i<s.size(); ++i)
	{
		if (seps.find(s[i]) != std::string::npos)
		{
			// this is a separator

			if (q)
			{
				// in quotes, add to current token
				token += s[i];
			}
			else if (!token.empty())
			{
				// not in quotes, push this token and reset
				v.push_back(token);
				token = "";
			}
		}
		else if (s[i] == '"')
		{
			// double quote

			if (q)
			{
				// end of quoted token
				q = false;

				if (!token.empty())
				{
					// push this token and reset
					v.push_back(token);
					token = "";
				}
			}
			else
			{
				// start of quoted token
				q = true;
			}
		}
		else
		{
			// not a separator, not a quote
			token += s[i];
		}
	}

	// last token
	if (!token.empty())
		v.push_back(token);

	return v;
}

template <class C>
void trim_impl(std::basic_string<C>& s, std::basic_string_view<C> what)
{
	while (!s.empty())
	{
		if (what.find(s[0]) != std::string::npos)
			s.erase(0, 1);
		else if (what.find(s[s.size() - 1]) != std::string::npos)
			s.erase(s.size() - 1, 1);
		else
			break;
	}
}

template <class C>
std::basic_string<C> trim_copy_impl(
	std::basic_string_view<C> s, std::basic_string_view<C> what)
{
	std::basic_string<C> c(s);
	trim(c, what);
	return c;
}


void trim(std::string& s, std::string_view what)
{
	trim_impl(s, what);
}

void trim(std::wstring& s, std::wstring_view what)
{
	trim_impl(s, what);
}

std::string trim_copy(std::string_view s, std::string_view what)
{
	return trim_copy_impl(s, what);
}

std::wstring trim_copy(std::wstring_view s, std::wstring_view what)
{
	return trim_copy_impl(s, what);
}


std::string pad_right(std::string s, std::size_t n, char c)
{
	if (s.size() < n)
		s.append(n - s.size() , c);

	return s;
}

std::string pad_left(std::string s, std::size_t n, char c)
{
	if (s.size() < n)
		s.insert(s.begin(), n - s.size() , c);

	return s;
}


std::string table(
	const std::vector<std::pair<std::string, std::string>>& v,
	std::size_t indent, std::size_t spacing)
{
	std::size_t longest = 0;

	for (auto&& p : v)
		longest = std::max(longest, p.first.size());

	std::string s;

	for (auto&& p : v)
	{
		if (!s.empty())
			s += "\n";

		s +=
			std::string(indent, ' ') +
			pad_right(p.first, longest) + " " +
			std::string(spacing, ' ') +
			p.second;
	}

	return s;
}


std::optional<std::wstring> to_widechar(UINT from, std::string_view s)
{
	std::wstring ws;

	if (s.empty())
		return ws;

	ws.resize(s.size() + 1);

	for (int t=0; t<3; ++t)
	{
		const int written = MultiByteToWideChar(
			from, 0, s.data(), static_cast<int>(s.size()),
			ws.data(), static_cast<int>(ws.size()));

		if (written <= 0)
		{
			const auto e = GetLastError();

			if (e == ERROR_INSUFFICIENT_BUFFER)
			{
				ws.resize(ws.size() * 2);
				continue;
			}
			else
			{
				return {};
			}
		}
		else
		{
			MOB_ASSERT(static_cast<std::size_t>(written) <= s.size());
			ws.resize(static_cast<std::size_t>(written));
			break;
		}
	}

	return ws;
}

std::optional<std::string> to_multibyte(UINT to, std::wstring_view ws)
{
	std::string s;

	if (ws.empty())
		return s;


	s.resize(static_cast<std::size_t>(
		static_cast<double>(ws.size()) * 1.5));

	for (int t=0; t<3; ++t)
	{
		const int written = WideCharToMultiByte(
			to, 0, ws.data(), static_cast<int>(ws.size()),
			s.data(), static_cast<int>(s.size()), nullptr, nullptr);

		if (written <= 0)
		{
			const auto e = GetLastError();

			if (e == ERROR_INSUFFICIENT_BUFFER)
			{
				s.resize(ws.size() * 2);
				continue;
			}
			else
			{
				return {};
			}
		}
		else
		{
			MOB_ASSERT(static_cast<std::size_t>(written) <= s.size());
			s.resize(static_cast<std::size_t>(written));
			break;
		}
	}

	return s;
}


std::wstring utf8_to_utf16(std::string_view s)
{
	auto ws = to_widechar(CP_UTF8, s);
	if (!ws)
	{
		std::wcerr << L"can't convert from utf8 to utf16\n";
		return L"???";
	}

	return std::move(*ws);
}

std::string utf16_to_utf8(std::wstring_view ws)
{
	auto s = to_multibyte(CP_UTF8, ws);
	if (!s)
	{
		std::wcerr << L"can't convert from utf16 to utf8\n";
		return "???";
	}

	return std::move(*s);
}

std::wstring cp_to_utf16(UINT from, std::string_view s)
{
	auto ws = to_widechar(from, s);
	if (!ws)
	{
		std::wcerr << L"can't convert from cp " << from << L" to utf16\n";
		return L"???";
	}

	return std::move(*ws);
}

std::string utf16_to_cp(UINT to, std::wstring_view ws)
{
	auto s = to_multibyte(to, ws);

	if (!s)
	{
		std::wcerr << L"can't convert from cp " << to << L" to utf16\n";
		return "???";
	}

	return std::move(*s);
}


std::string bytes_to_utf8(encodings e, std::string_view s)
{
	switch (e)
	{
		case encodings::utf16:
		{
			const auto* ws = reinterpret_cast<const wchar_t*>(s.data());
			const auto chars = s.size() / sizeof(wchar_t);
			return utf16_to_utf8({ws, chars});
		}

		case encodings::acp:
		{
			const std::wstring utf16 = cp_to_utf16(CP_ACP, s);
			return utf16_to_utf8(utf16);
		}

		case encodings::oem:
		{
			const std::wstring utf16 = cp_to_utf16(CP_OEMCP, s);
			return utf16_to_utf8(utf16);
		}

		case encodings::utf8:
		case encodings::dont_know:
		default:
		{
			return {s.begin(), s.end()};
		}
	}
}

std::string utf16_to_bytes(encodings e, std::wstring_view ws)
{
	switch (e)
	{
		case encodings::utf16:
		{
			return std::string(
				reinterpret_cast<const char*>(ws.data()),
				ws.size() * sizeof(wchar_t));
		}

		case encodings::acp:
		{
			return utf16_to_cp(CP_ACP, ws);
		}

		case encodings::oem:
		{
			return utf16_to_cp(CP_OEMCP, ws);
		}

		case encodings::utf8:
		case encodings::dont_know:
		default:
		{
			return utf16_to_utf8(ws);
		}
	}
}

std::string utf8_to_bytes(encodings e, std::string_view utf8)
{
	switch (e)
	{
		case encodings::utf16:
		case encodings::acp:
		case encodings::oem:
		{
			const std::wstring ws = utf8_to_utf16(utf8);
			return utf16_to_bytes(e, ws);
		}

		case encodings::utf8:
		case encodings::dont_know:
		default:
		{
			return std::string(utf8);
		}
	}
}

std::string path_to_utf8(fs::path p)
{
	return utf16_to_utf8(p.native());
}


encoded_buffer::encoded_buffer(encodings e, std::string bytes)
	: e_(e), bytes_(std::move(bytes)), last_(0)
{
}

void encoded_buffer::add(std::string_view bytes)
{
	bytes_.append(bytes.begin(), bytes.end());
}

std::string encoded_buffer::utf8_string() const
{
	return bytes_to_utf8(e_, bytes_);
}

} // namespace

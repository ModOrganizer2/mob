#include "pch.h"
#include "utility.h"
#include "conf.h"
#include "op.h"
#include "net.h"
#include "process.h"
#include "context.h"

namespace mob
{

extern u8stream u8cout(std::wcout);
extern u8stream u8cerr(std::wcerr);


void u8stream::do_output(const std::string& s)
{
	out_ << utf8_to_utf16(s);
}


void mob_assertion_failed(
	const char* message,
	const char* exp, const char* file, int line, const char* func)
{
	if (message)
	{
		gcx().error(context::generic,
			"assertion failed: {}:{} {}: {} ({})",
			file, line, func, message, exp);
	}
	else
	{
		gcx().error(context::generic,
			"assertion failed: {}:{} {}: '{}'",
			file, line, func, exp);
	}

	if (IsDebuggerPresent())
		DebugBreak();
}

url make_github_url(const std::string& org, const std::string& repo)
{
	return "https://github.com/" + org + "/" + repo + ".git";
}

url make_prebuilt_url(const std::string& filename)
{
	return
		"https://github.com/ModOrganizer2/modorganizer-umbrella/"
		"releases/download/1.1/" + filename;
}

url make_appveyor_artifact_url(
	arch a, const std::string& project, const std::string& filename)
{
	std::string arch_s;

	switch (a)
	{
		case arch::x86:
			arch_s = "x86";
			break;

		case arch::x64:
			arch_s = "x64";
			break;

		case arch::dont_care:
		default:
			gcx().bail_out(context::generic, "bad arch");
	}

	return
		"https://ci.appveyor.com/api/projects/Modorganizer2/" +
		project + "/artifacts/" + filename + "?job=Platform:%20" + arch_s;
}

std::string replace_all(
	std::string s, const std::string& from, const std::string& to)
{
	for (;;)
	{
		const auto pos = s.find(from);
		if (pos == std::string::npos)
			break;

		s.replace(pos, from.size(), to);
	}

	return s;
}

std::string join(const std::vector<std::string>& v, const std::string& sep)
{
	std::string s;

	for (auto&& e : v)
	{
		if (!s.empty())
			s += sep;

		s += e;
	}

	return s;
}

void trim(std::string& s, const std::string& what)
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

std::string trim_copy(const std::string& s, const std::string& what)
{
	std::string c = s;
	trim(c, what);
	return c;
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


file_deleter::file_deleter(const context& cx, fs::path p)
	: cx_(cx), p_(std::move(p)), delete_(true)
{
	cx_.trace(context::fs, "will delete {} if things go bad", p_);
}

file_deleter::~file_deleter()
{
	try
	{
		if (delete_)
			delete_now();
	}
	catch(...)
	{
		// eat it
	}
}

void file_deleter::delete_now()
{
	cx_.debug(context::fs, "something went bad, deleting {}", p_);
	op::delete_file(cx_, p_, op::optional);
}

void file_deleter::cancel()
{
	cx_.trace(context::fs, "everything okay, keeping {}", p_);
	delete_ = false;
}


directory_deleter::directory_deleter(const context& cx, fs::path p)
	: cx_(cx), p_(std::move(p)), delete_(true)
{
	cx_.trace(context::fs, "will delete {} if things go bad", p_);
}

directory_deleter::~directory_deleter()
{
	try
	{
		if (delete_)
			delete_now();
	}
	catch(...)
	{
		// eat it
	}
}

void directory_deleter::delete_now()
{
	cx_.debug(context::fs, "something went bad, deleting {}", p_);
	op::delete_directory(cx_, p_, op::optional);
}

void directory_deleter::cancel()
{
	cx_.trace(context::fs, "everything okay, keeping {}", p_);
	delete_ = false;
}


interruption_file::interruption_file(
	const context& cx, fs::path dir, std::string name)
		: cx_(cx), dir_(std::move(dir)), name_(std::move(name))
{
	if (fs::exists(file()))
		cx_.trace(context::interruption, "found interrupt file {}", file());
}

bool interruption_file::exists() const
{
	return fs::exists(file());
}

fs::path interruption_file::file() const
{
	return dir_ / ("_mo_interrupted_" + name_);
}

void interruption_file::create()
{
	cx_.trace(context::interruption, "creating interrupt file {}", file());
	op::touch(cx_, file());
}

void interruption_file::remove()
{
	cx_.trace(context::interruption, "removing interrupt file {}", file());
	op::delete_file(cx_, file());
}


bypass_file::bypass_file(const context& cx, fs::path dir, std::string name)
	: cx_(cx), file_(dir / ("_mob_" + name))
{
}

bool bypass_file::exists() const
{
	if (fs::exists(file_))
	{
		if (conf::rebuild())
		{
			cx_.trace(context::rebuild,
				"bypass file {} exists, deleting", file_);

			op::delete_file(cx_, file_, op::optional);

			return false;
		}
		else
		{
			cx_.trace(context::bypass, "bypass file {} exists", file_);
			return true;
		}
	}
	else
	{
		cx_.trace(context::bypass, "bypass file {} not found", file_);
		return false;
	}
}

void bypass_file::create()
{
	cx_.trace(context::bypass, "create bypass file {}", file_);
	op::touch(cx_, file_);
}


enum class color_methods
{
	none = 0,
	ansi,
	console
};

static color_methods g_color_method = []
{
	DWORD d = 0;
	if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &d))
	{
		if ((d & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0)
			return color_methods::console;
		else
			return color_methods::ansi;
	}

	return color_methods::none;
}();

console_color::console_color()
	: reset_(false), old_atts_(0)
{
}

console_color::console_color(colors c)
	: reset_(false), old_atts_(0)
{
	if (g_color_method == color_methods::ansi)
	{
		switch (c)
		{
			case colors::white:
				break;

			case colors::grey:
				reset_ = true;
				u8cout << "\033[38;2;150;150;150m";
				break;

			case colors::yellow:
				reset_ = true;
				u8cout << "\033[38;2;240;240;50m";
				break;

			case colors::red:
				reset_ = true;
				u8cout << "\033[38;2;240;50;50m";
				break;
		}
	}
	else if (g_color_method == color_methods::console)
	{
		CONSOLE_SCREEN_BUFFER_INFO bi = {};
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bi);
		old_atts_ = bi.wAttributes;

		WORD atts = 0;

		switch (c)
		{
			case colors::white:
				break;

			case colors::grey:
				reset_ = true;
				atts = FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED;
				break;

			case colors::yellow:
				reset_ = true;
				atts = FOREGROUND_GREEN|FOREGROUND_RED;
				break;

			case colors::red:
				reset_ = true;
				atts = FOREGROUND_RED;
				break;
		}

		if (atts != 0)
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), atts);
	}
}

console_color::~console_color()
{
	if (!reset_)
		return;

	if (g_color_method == color_methods::ansi)
	{
		u8cout << "\033[39m\033[49m";
	}
	else if (g_color_method == color_methods::console)
	{
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), old_atts_);
	}
}


std::optional<std::wstring> to_utf16(UINT from, std::string_view s)
{
	if (s.empty())
		return std::wstring();

	const int wsize = MultiByteToWideChar(
		from, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);

	if (wsize == 0)
		return {};

	auto buffer = std::make_unique<wchar_t[]>(
		static_cast<std::size_t>(wsize + 1));

	const int written = MultiByteToWideChar(
		from, 0, s.data(), static_cast<int>(s.size()),
		buffer.get(), wsize);

	if (written == 0)
		return {};

	MOB_ASSERT(written == wsize);

	return std::wstring(buffer.get(), buffer.get() + written);
}

std::optional<std::string> to_utf8(std::wstring_view ws)
{
	if (ws.empty())
		return std::string();

	const int size = WideCharToMultiByte(
		CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0,
		nullptr, nullptr);

	if (size == 0)
		return {};

	auto buffer = std::make_unique<char[]>(
		static_cast<std::size_t>(size + 1));

	const int written = WideCharToMultiByte(
		CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
		buffer.get(), size, nullptr, nullptr);

	if (written == 0)
		return {};

	MOB_ASSERT(written == size);

	return std::string(buffer.get(), buffer.get() + written);
}


std::wstring utf8_to_utf16(std::string_view s)
{
	auto ws = to_utf16(CP_UTF8, s);
	if (!ws)
	{
		std::cerr << "can't convert from utf8 to utf16\n";
		return L"???";
	}

	return *ws;
}

std::string utf16_to_utf8(std::wstring_view ws)
{
	auto s = to_utf8(ws);
	if (!s)
	{
		std::cerr << "can't convert from utf16 to utf8\n";
		return "???";
	}

	return *s;
}

std::wstring cp_to_utf16(std::string_view s)
{
	auto ws = to_utf16(CP_ACP, s);
	if (!ws)
	{
		std::cerr << "can't convert from acp to utf16\n";
		return L"???";
	}

	return *ws;
}

std::string cp_to_utf8(std::string_view s)
{
	auto ws = cp_to_utf16(s);
	auto s8 = to_utf8(ws);

	if (!s8)
	{
		std::cerr << "can't convert from acp to utf8\n";
		return "???";
	}

	return *s8;
}

std::string path_to_utf8(fs::path p)
{
	return utf16_to_utf8(p.native());
}

}	// namespace

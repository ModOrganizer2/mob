#include "pch.h"
#include "utility.h"
#include "conf.h"
#include "op.h"
#include "net.h"
#include "process.h"
#include "context.h"

namespace mob
{

static std::mutex g_output_mutex;

extern u8stream u8cout(false);
extern u8stream u8cerr(true);

constexpr std::size_t max_name_length = 1000;
constexpr std::size_t max_frames = 100;
const std::size_t exception_message_length = 5000;

static void* frame_addresses[max_frames];
static wchar_t undecorated_name[max_name_length + 1] = {};
static unsigned char sym_buffer[sizeof(SYMBOL_INFOW) + max_name_length];
static SYMBOL_INFOW* sym = (SYMBOL_INFOW*)sym_buffer;
static wchar_t exception_message[exception_message_length + 1] = {};

static LPTOP_LEVEL_EXCEPTION_FILTER g_previous_handler = nullptr;

void dump_stacktrace(const wchar_t* what)
{
	std::scoped_lock lock(g_output_mutex);

	std::wcerr
		<< "\n\nmob has crashed\n"
		<< L"*****************************\n\n"
		<< what << L"\n\n";


	HANDLE process = INVALID_HANDLE_VALUE;

	DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(),
		GetCurrentProcess(), &process, 0, false, DUPLICATE_SAME_ACCESS);

	SymSetOptions(SymGetOptions() & (~SYMOPT_UNDNAME));
	SymInitializeW(process, NULL, TRUE);

	const std::size_t frame_count = CaptureStackBackTrace(
		0, max_frames, frame_addresses, nullptr);

	for (std::size_t i=0; i<frame_count; ++i)
	{
		DWORD disp = 0;
		IMAGEHLP_LINEW64 line = {0};
		line.SizeOfStruct = sizeof(line);

		std::wcerr << frame_addresses[i] << L" ";

		if (SymGetLineFromAddrW64(
			process, reinterpret_cast<DWORD64>(frame_addresses[i]),
			&disp, &line))
		{
			std::wcerr << line.FileName << L":" << line.LineNumber << L" ";
		}

		DWORD64 disp2 = 0;

		sym->MaxNameLen = max_name_length;
		sym->SizeOfStruct = sizeof(SYMBOL_INFOW);

		if (SymFromAddrW(process, reinterpret_cast<DWORD64>(frame_addresses[i]), &disp2, sym))
		{
			const DWORD und_length = UnDecorateSymbolNameW(
				sym->Name, undecorated_name, max_name_length, UNDNAME_COMPLETE);

			std::wcerr << undecorated_name;
		}

		std::wcerr << L"\n";
	}

	if (IsDebuggerPresent())
		DebugBreak();
	else
		TerminateProcess(GetCurrentProcess(), 0xffff);
}

void terminate_handler() noexcept
{
	try
	{
		std::rethrow_exception(std::current_exception());
	}
	catch(std::exception& e)
	{
		auto* p = exception_message;
		std::size_t remaining = exception_message_length;

		const int n = _snwprintf(
			p, remaining, L"%s", L"unhandled exception: ");

		if (n >= 0)
		{
			p += n;
			remaining -=n ;
		}

		::MultiByteToWideChar(
			CP_ACP, 0, e.what(), -1, p, static_cast<int>(remaining));
	}
	catch(...)
	{
		auto* p = exception_message;
		std::size_t remaining = exception_message_length;

		_snwprintf(
			p, remaining, L"%s", L"unhandled exception");
	}

	dump_stacktrace(exception_message);
}

const wchar_t* error_code_name(DWORD code)
{
	switch (code)
	{
		case EXCEPTION_ACCESS_VIOLATION:         return L"EXCEPTION_ACCESS_VIOLATION";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return L"EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
		case EXCEPTION_BREAKPOINT:               return L"EXCEPTION_BREAKPOINT";
		case EXCEPTION_DATATYPE_MISALIGNMENT:    return L"EXCEPTION_DATATYPE_MISALIGNMENT";
		case EXCEPTION_FLT_DENORMAL_OPERAND:     return L"EXCEPTION_FLT_DENORMAL_OPERAND";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return L"EXCEPTION_FLT_DIVIDE_BY_ZERO";
		case EXCEPTION_FLT_INEXACT_RESULT:       return L"EXCEPTION_FLT_INEXACT_RESULT";
		case EXCEPTION_FLT_INVALID_OPERATION:    return L"EXCEPTION_FLT_INVALID_OPERATION";
		case EXCEPTION_FLT_OVERFLOW:             return L"EXCEPTION_FLT_OVERFLOW";
		case EXCEPTION_FLT_STACK_CHECK:          return L"EXCEPTION_FLT_STACK_CHECK";
		case EXCEPTION_FLT_UNDERFLOW:            return L"EXCEPTION_FLT_UNDERFLOW";
		case EXCEPTION_ILLEGAL_INSTRUCTION:      return L"EXCEPTION_ILLEGAL_INSTRUCTION";
		case EXCEPTION_IN_PAGE_ERROR:            return L"EXCEPTION_IN_PAGE_ERROR";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:       return L"EXCEPTION_INT_DIVIDE_BY_ZERO";
		case EXCEPTION_INT_OVERFLOW:             return L"EXCEPTION_INT_OVERFLOW";
		case EXCEPTION_INVALID_DISPOSITION:      return L"EXCEPTION_INVALID_DISPOSITION";
		case EXCEPTION_NONCONTINUABLE_EXCEPTION: return L"EXCEPTION_NONCONTINUABLE_EXCEPTION";
		case EXCEPTION_PRIV_INSTRUCTION:         return L"EXCEPTION_PRIV_INSTRUCTION";
		case EXCEPTION_SINGLE_STEP:              return L"EXCEPTION_SINGLE_STEP";
		case EXCEPTION_STACK_OVERFLOW:           return L"EXCEPTION_STACK_OVERFLOW";
		default:                                 return L"unknown exception" ;
	}
}

LONG WINAPI unhandled_exception_handler(LPEXCEPTION_POINTERS ep) noexcept
{
	if (ep->ExceptionRecord->ExceptionCode == 0xE06D7363)
	{
		if (g_previous_handler)
			return g_previous_handler(ep);;
	}

	wchar_t* p = exception_message;
	std::size_t remaining = exception_message_length;

	const auto n = GetModuleFileNameW(
		GetModuleHandleW(nullptr), exception_message,
		exception_message_length);

	p += n;
	remaining -= n;

	const auto n2 = _snwprintf(
		p, remaining, L": exception thrown at 0x%p: 0x%lX %s",
		ep->ExceptionRecord->ExceptionAddress,
		ep->ExceptionRecord->ExceptionCode,
		error_code_name(ep->ExceptionRecord->ExceptionCode));

	p += n2;
	remaining -= n2;


	dump_stacktrace(exception_message);
	return EXCEPTION_CONTINUE_SEARCH;
}

void set_thread_exception_handlers()
{
	g_previous_handler = SetUnhandledExceptionFilter(
		mob::unhandled_exception_handler);

	std::set_terminate(mob::terminate_handler);
}


static bool stdout_console = []
{
	DWORD d = 0;

	if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &d))
	{
		// this is a console
		return true;
	}

	return false;
}();

static bool stderr_console = []
{
	DWORD d = 0;

	if (GetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), &d))
	{
		// this is a console
		return true;
	}

	return false;
}();


void set_std_streams()
{
	if (stdout_console)
		_setmode(_fileno(stdout), _O_U16TEXT);

	if (stderr_console)
		_setmode(_fileno(stderr), _O_U16TEXT);
}

void u8stream::do_output(const std::string& s)
{
	std::scoped_lock lock(g_output_mutex);

	if (err_)
	{
		if (stderr_console)
			std::wcerr << utf8_to_utf16(s);
		else
			std::cerr << s;
	}
	else
	{
		if (stdout_console)
			std::wcout << utf8_to_utf16(s);
		else
			std::cout << s;
	}
}

void u8stream::write_ln(std::string_view utf8)
{
	std::scoped_lock lock(g_output_mutex);

	if (err_)
	{
		if (stderr_console)
			std::wcerr << utf8_to_utf16(utf8) << L"\n";
		else
			std::cerr << utf8 << "\n";
	}
	else
	{
		if (stdout_console)
			std::wcout << utf8_to_utf16(utf8) << L"\n";
		else
			std::cout << utf8 << "\n";
	}
}


void mob_assertion_failed(
	const char* message,
	const char* exp, const wchar_t* file, int line, const char* func)
{
	if (message)
	{
		gcx().error(context::generic,
			"assertion failed: {}:{} {}: {} ({})",
			std::wstring(file), line, func, message, exp);
	}
	else
	{
		gcx().error(context::generic,
			"assertion failed: {}:{} {}: '{}'",
			std::wstring(file), line, func, exp);
	}

	if (IsDebuggerPresent())
		DebugBreak();
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

bool glob_match(const std::string& pattern, const std::string& s)
{
	try
	{
		return std::regex_match(s, std::regex(replace_all(pattern, "*", ".*")));
	}
	catch(std::exception&)
	{
		u8cerr
			<< "globs are actually bastardized regexes where '*' is replaced "
			<< "by '.*', so don't push it\n";

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


file_deleter::file_deleter(const context& cx, fs::path p)
	: cx_(cx), p_(std::move(p)), delete_(true)
{
	cx_.trace(context::fs, "will delete {} if things go bad", p_);
}

file_deleter::~file_deleter()
{
	if (delete_)
		delete_now();
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
	if (delete_)
		delete_now();

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

	s.resize(static_cast<std::size_t>(ws.size() * 1.5));

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

}	// namespace

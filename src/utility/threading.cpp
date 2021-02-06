#include "pch.h"
#include "threading.h"
#include "../utility.h"

namespace mob
{

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
	// don't use 8ucout, don't lock the global out mutex, this can be called
	// while the mutex is locked
	std::wcerr << what
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


std::size_t make_thread_count(std::optional<std::size_t> count)
{
	static const auto def = std::thread::hardware_concurrency();
	return std::max<std::size_t>(1, count.value_or(def));
}


thread_pool::thread_pool(std::optional<std::size_t> count)
	: count_(make_thread_count(count))
{
	for (std::size_t i=0; i<count_; ++i)
		threads_.emplace_back(std::make_unique<thread_info>());
}

thread_pool::~thread_pool()
{
	join();
}

void thread_pool::join()
{
	for (auto&& t : threads_)
	{
		if (t->thread.joinable())
			t->thread.join();
	}
}

void thread_pool::add(fun thread_fun)
{
	for (;;)
	{
		if (try_add(thread_fun))
			break;

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

bool thread_pool::try_add(fun thread_fun)
{
	for (auto& t : threads_)
	{
		if (t->running)
			continue;

		// found one

		if (t->thread.joinable())
			t->thread.join();

		t->running = true;
		t->thread_fun = thread_fun;

		t->thread = start_thread([&]
		{
			t->thread_fun();
			t->running = false;
		});

		return true;
	}

	return false;
}

} // namespace

#include "pch.h"
#include "utility.h"
#include "net.h"
#include "core/conf.h"
#include "core/op.h"
#include "core/process.h"
#include "core/context.h"

namespace mob
{

static std::mutex g_output_mutex;

extern u8stream u8cout(false);
extern u8stream u8cerr(true);


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

std::mutex& global_output_mutex()
{
	return g_output_mutex;
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



}	// namespace

#include "pch.h"
#include "io.h"
#include "string.h"

namespace mob {

    enum class color_methods { none = 0, ansi, console };

    // returns whether the given standard handle is for a console or is redirected
    // somewhere else
    //
    bool is_handle_console(DWORD handle)
    {
        DWORD d = 0;

        if (GetConsoleMode(GetStdHandle(handle), &d)) {
            // this is a console
            return true;
        }

        return false;
    }

    // figures out if the terminal supports ansi color codes; the old conhost
    // doesn't, but the new terminal does
    //
    // returns color_methods::none if the output is not a console
    //
    color_methods get_color_method()
    {
        DWORD d = 0;

        if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &d)) {
            if ((d & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0)
                return color_methods::console;
            else
                return color_methods::ansi;
        }

        // not a console
        return color_methods::none;
    }

    // global output mutex, avoids interleaving
    static std::mutex g_output_mutex;

    // streams
    extern u8stream u8cout(false);
    extern u8stream u8cerr(true);

    // whether stdout and stderr are for console, only check once
    static bool stdout_console = is_handle_console(STD_OUTPUT_HANDLE);
    static bool stderr_console = is_handle_console(STD_ERROR_HANDLE);

    // color method supported by terminal, only check once
    static color_methods g_color_method = get_color_method();

    void set_std_streams()
    {
        // only set to utf16 when the output is the console

        if (stdout_console)
            _setmode(_fileno(stdout), _O_U16TEXT);

        if (stderr_console)
            _setmode(_fileno(stderr), _O_U16TEXT);
    }

    std::mutex& global_output_mutex()
    {
        return g_output_mutex;
    }

    yn ask_yes_no(const std::string& text, yn def)
    {
        u8cout << text << (text.empty() ? "" : " ")
               << (def == yn::yes ? "[Y/n]" : "[y/N]") << " ";

        // stdin is not utf8
        std::string line;
        std::getline(std::cin, line);

        // ctrl+c
        if (!std::cin)
            return yn::cancelled;

        if (line.empty())
            return def;
        else if (line == "y" || line == "Y")
            return yn::yes;
        else if (line == "n" || line == "N")
            return yn::no;
        else
            return yn::cancelled;
    }

    void u8stream::do_output(const std::string& s)
    {
        std::scoped_lock lock(g_output_mutex);

        if (err_) {
            if (stderr_console)
                std::wcerr << utf8_to_utf16(s);
            else
                std::cerr << s;
        }
        else {
            if (stdout_console)
                std::wcout << utf8_to_utf16(s);
            else
                std::cout << s;
        }
    }

    void u8stream::write_ln(std::string_view utf8)
    {
        std::scoped_lock lock(g_output_mutex);

        if (err_) {
            if (stderr_console)
                std::wcerr << utf8_to_utf16(utf8) << L"\n";
            else
                std::cerr << utf8 << "\n";
        }
        else {
            if (stdout_console)
                std::wcout << utf8_to_utf16(utf8) << L"\n";
            else
                std::cout << utf8 << "\n";
        }
    }

    console_color::console_color() : reset_(false), old_atts_(0) {}

    console_color::console_color(colors c) : reset_(false), old_atts_(0)
    {
        if (g_color_method == color_methods::ansi) {
            switch (c) {
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
        else if (g_color_method == color_methods::console) {
            CONSOLE_SCREEN_BUFFER_INFO bi = {};
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bi);
            old_atts_ = bi.wAttributes;

            WORD atts = 0;

            switch (c) {
            case colors::white:
                break;

            case colors::grey:
                reset_ = true;
                atts   = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
                break;

            case colors::yellow:
                reset_ = true;
                atts   = FOREGROUND_GREEN | FOREGROUND_RED;
                break;

            case colors::red:
                reset_ = true;
                atts   = FOREGROUND_RED;
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

        if (g_color_method == color_methods::ansi) {
            u8cout << "\033[39m\033[49m";
        }
        else if (g_color_method == color_methods::console) {
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), old_atts_);
        }
    }

    font_restorer::font_restorer() : restore_(false)
    {
        std::memset(&old_, 0, sizeof(old_));
        old_.cbSize = sizeof(old_);

        if (GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &old_))
            restore_ = true;
    }

    font_restorer::~font_restorer()
    {
        if (!restore_)
            return;

        CONSOLE_FONT_INFOEX now = {};
        now.cbSize              = sizeof(now);

        if (!GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &now))
            return;

        if (std::wcsncmp(old_.FaceName, now.FaceName, LF_FACESIZE) != 0)
            restore();
    }

    void font_restorer::restore()
    {
        ::SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &old_);
    }

}  // namespace mob

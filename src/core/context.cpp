#include "pch.h"
#include "context.h"
#include "../tasks/task.h"
#include "../tools/tools.h"
#include "../utility.h"
#include "conf.h"

namespace mob::details {

    std::string converter<std::wstring>::convert(const std::wstring& s)
    {
        return utf16_to_utf8(s);
    }

    std::string converter<fs::path>::convert(const fs::path& s)
    {
        return utf16_to_utf8(s.native());
    }

    std::string converter<url>::convert(const url& u)
    {
        return u.string();
    }

}  // namespace mob::details

namespace mob {

    // timestamps are relative to this
    static hr_clock::time_point g_start_time = hr_clock::now();

    // accumulated errors and warnings; only used if should_dump_logs() is true,
    // dumped on the console just before mob exits
    static std::vector<std::string> g_errors, g_warnings;

    // handle to log file
    static handle_ptr g_log_file;

    // global output mutex to avoid interleaving, but also mixing colors
    static std::mutex g_mutex;

    // returns the color associated with the given level
    //
    console_color level_color(context::level lv)
    {
        switch (lv) {
        case context::level::dump:
        case context::level::trace:
        case context::level::debug:
            return console_color::grey;

        case context::level::warning:
            return console_color::yellow;

        case context::level::error:
            return console_color::red;

        case context::level::info:
        default:
            return console_color::white;
        }
    }

    // converts a reason to string
    //
    const char* reason_string(context::reason r)
    {
        switch (r) {
        case context::bypass:
            return "bypass";
        case context::redownload:
            return "re-dl";
        case context::rebuild:
            return "re-bd";
        case context::reextract:
            return "re-ex";
        case context::interruption:
            return "int";
        case context::cmd:
            return "cmd";
        case context::std_out:
            return "stdout";
        case context::std_err:
            return "stderr";
        case context::fs:
            return (conf().global().dry() ? "fs-dry" : "fs");
        case context::net:
            return "net";
        case context::generic:
            return "";
        case context::conf:
            return "conf";
        default:
            return "?";
        }
    }

    // retrieves the error message from the system for the given id
    //
    std::string error_message(DWORD id)
    {
        wchar_t* message = nullptr;

        const auto ret =
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           reinterpret_cast<LPWSTR>(&message), 0, NULL);

        std::wstring s;

        std::wostringstream oss;

        // hex error code
        oss << L"0x" << std::hex << id;

        if (ret == 0 || !message) {
            // error message not found, just use the hex error code
            s = oss.str();
        }
        else {
            // FormatMessage() includes a newline, trim it and put the hex code too
            s = trim_copy(message) + L" (" + oss.str() + L")";
        }

        LocalFree(message);

        return utf16_to_utf8(s);
    }

    std::chrono::nanoseconds timestamp()
    {
        return (hr_clock::now() - g_start_time);
    }

    std::string_view timestamp_string()
    {
        // thread local buffer to avoid allocation
        static thread_local char buffer[50];

        using namespace std::chrono;

        // getting time in seconds as a float
        const auto ms   = duration_cast<milliseconds>(timestamp());
        const auto frac = static_cast<float>(ms.count()) / 1000.0;

        // to string with 2 digits precision
        const auto r = std::to_chars(std::begin(buffer), std::end(buffer), frac,
                                     std::chars_format::fixed, 2);

        if (r.ec != std::errc())
            return "?";

        return {buffer, r.ptr};
    }

    // returns if the given level should be enabled based on the given level from
    // the ini
    //
    // unfortunately, log levels in the ini have dump as the highest number, but
    // the level enum is the reverse
    //
    bool log_enabled(context::level lv, int conf_lv)
    {
        switch (lv) {
        case context::level::dump:
            return conf_lv > 5;

        case context::level::trace:
            return conf_lv > 4;

        case context::level::debug:
            return conf_lv > 3;

        case context::level::info:
            return conf_lv > 2;

        case context::level::warning:
            return conf_lv > 1;

        case context::level::error:
            return conf_lv > 0;

        default:
            return true;
        }
    }

    // whether errors and warnings should be dumped at the end, only returns true
    // for debug level and higher, lower levels don't have enough stuff on the
    // console to make it worth, it's just annoying to have duplicate logs
    //
    // in fact, this feature might not be very useful at all
    //
    bool should_dump_logs()
    {
        return log_enabled(context::level::debug, conf().global().output_log_level());
    }

    context::context(std::string task_name)
        : task_(std::move(task_name)), tool_(nullptr)
    {
    }

    void context::set_tool(tool* t)
    {
        tool_ = t;
    }

    const context* context::global()
    {
        static thread_local context c("");
        return &c;
    }

    bool context::enabled(level lv)
    {
        // a log level is enabled if it's included in either the console or the log
        // file, which have independent levels
        const int minimum_log_level = std::max(mob::conf().global().output_log_level(),
                                               mob::conf().global().file_log_level());

        return log_enabled(lv, minimum_log_level);
    }

    void context::set_log_file(const fs::path& p)
    {
        if (!mob::conf().global().dry() && !p.empty()) {
            // creating directory
            if (!exists(p.parent_path()))
                op::create_directories(gcx(), mob::conf().path().prefix());

            HANDLE h = CreateFileW(p.native().c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                   nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

            if (h == INVALID_HANDLE_VALUE) {
                const auto e = GetLastError();
                gcx().bail_out(context::generic, "failed to open log file {}, {}", p,
                               error_message(e));
            }

            g_log_file.reset(h);
        }
    }

    void context::close_log_file()
    {
        g_log_file.reset();
    }

    void context::log_string(reason r, level lv, std::string_view s) const
    {
        if (!enabled(lv))
            return;

        do_log_impl(false, r, lv, s);
    }

    void context::do_log_impl(bool bail, reason r, level lv,
                              std::string_view utf8) const
    {
        std::string_view sv = make_log_string(r, lv, utf8);

        if (bail) {
            // log the string with "(bailing out)" at the end, but throw the
            // original, it's prettier that way
            const std::string s(sv);
            emit_log(lv, s + " (bailing out)");
            throw bailed(s);
        }
        else {
            emit_log(lv, sv);
        }
    }

    void context::emit_log(level lv, std::string_view utf8) const
    {
        std::scoped_lock lock(g_mutex);

        // console
        if (log_enabled(lv, mob::conf().global().output_log_level())) {
            // will revert color in dtor
            console_color c = level_color(lv);
            u8cout.write_ln(utf8);
        }

        // log file
        if (g_log_file && log_enabled(lv, mob::conf().global().file_log_level())) {
            DWORD written = 0;

            ::WriteFile(g_log_file.get(), utf8.data(), static_cast<DWORD>(utf8.size()),
                        &written, nullptr);

            ::WriteFile(g_log_file.get(), "\r\n", 2, &written, nullptr);
        }

        // remember warnings and errors
        if (should_dump_logs()) {
            if (lv == level::error)
                g_errors.emplace_back(utf8);
            else if (lv == level::warning)
                g_warnings.emplace_back(utf8);
        }
    }

    // used by make_log_string(), appends `what` to `s`, with padding on the right
    // up to `max_length` characters, including the length of `what`
    //
    void append(std::string& s, std::string_view what, std::size_t max_length)
    {
        if (what.empty()) {
            s.append(max_length, ' ');
        }
        else {
            s.append(what);

            // padding
            const std::size_t written = what.size();
            if (written < max_length)
                s.append(max_length - written, ' ');
        }
    }

    // used by make_log_string(), same as append() above but puts `what` between
    // [brackets]
    //
    // avoids unnecessary memory allocations by appending the brackets directly
    //
    void append_with_brackets(std::string& s, std::string_view what,
                              std::size_t max_length)
    {
        if (what.empty()) {
            s.append(max_length, ' ');
        }
        else {
            s.append(1, '[');
            s.append(what);
            s.append(1, ']');

            // padding
            const std::size_t written = what.size() + 2;  // "[what]"
            if (written < max_length)
                s.append(max_length - written, ' ');
        }
    }

    // used by make_log_string(), can append some stuff depending on the reason
    //
    void append_context(std::string& ls, context::reason r)
    {
        switch (r) {
        case context::redownload: {
            ls.append(" (happened because of --redownload)");
            break;
        }

        case context::rebuild: {
            ls.append(" (happened because of --rebuild)");
            break;
        }

        case context::reextract: {
            ls.append(" (happened because of --reextract)");
            break;
        }

        case context::cmd:
        case context::bypass:
        case context::std_out:
        case context::std_err:
        case context::fs:
        case context::net:
        case context::generic:
        case context::conf:
        case context::interruption:
        default:
            break;
        }
    }

    std::string_view context::make_log_string(reason r, level, std::string_view s) const
    {
        // maximum lengths of the various components below, used for padding

        // mob shouldn't run for more than three hours, includes space
        const std::size_t timestamp_max_length = 8;  // '0000.00 '

        // cut task name at 15, +3 because brackets + space at the end
        const std::size_t longest_task_name    = 15;
        const std::size_t task_name_max_length = longest_task_name + 3;

        // cut tool name at 7, +3 because brackets + space at the end
        const std::size_t longest_tool_name    = 7;
        const std::size_t tool_name_max_length = longest_tool_name + 3;

        // cut reason name at 7, +3 because brackets + space at the end
        const std::size_t longest_reason    = 7;
        const std::size_t reason_max_length = longest_reason + 3;

        // keep a thread local string to avoid memory allocations
        static thread_local std::string ls;

        // clear previous log
        ls.clear();

        // a full log line might look like:
        //   "2.77     [cmake_common]    [git]     [cmd]     creating process"

        // timestamp
        append(ls, timestamp_string(), timestamp_max_length);

        // task name
        append_with_brackets(ls, task_.substr(0, longest_task_name),
                             task_name_max_length);

        // tool
        if (tool_) {
            append_with_brackets(ls, tool_->name().substr(0, longest_tool_name),
                                 tool_name_max_length);
        }
        else {
            ls.append(tool_name_max_length, ' ');
        }

        // reason
        append_with_brackets(ls, reason_string(r), reason_max_length);

        // log message
        ls.append(s);

        // context
        append_context(ls, r);

        return ls;
    }

    void dump_logs()
    {
        if (!should_dump_logs())
            return;

        if (!g_warnings.empty() || !g_errors.empty()) {
            u8cout << "\n\nthere were problems:\n";

            {
                auto c = level_color(context::level::warning);
                for (auto&& s : g_warnings)
                    u8cout << s << "\n";
            }

            {
                auto c = level_color(context::level::error);
                for (auto&& s : g_errors)
                    u8cout << s << "\n";
            }
        }
    }

}  // namespace mob

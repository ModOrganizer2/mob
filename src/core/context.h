#pragma once

#include "../utility.h"

// T to std::string converters
//
// those are kept in this namespace so they don't leak all over the place;
// they're used directly by context::do_log() below

namespace mob::details {

    class mob::url;

    template <class T, class = void>
    struct converter {
        static const T& convert(const T& t) { return t; }
    };

    template <>
    struct converter<std::wstring> {
        static std::string convert(const std::wstring& s);
    };

    template <>
    struct converter<fs::path> {
        static std::string convert(const fs::path& s);
    };

    template <>
    struct converter<url> {
        static std::string convert(const url& u);
    };

    template <class T>
    struct converter<T, std::enable_if_t<std::is_enum_v<T>>> {
        static std::string convert(T e)
        {
            return std::to_string(static_cast<std::underlying_type_t<T>>(e));
        }
    };

}  // namespace mob::details

namespace mob {

    class tool;

    // system error message
    //
    std::string error_message(DWORD e);

    // a logger with some context, this is passed around everywhere and knows which
    // task and tool is currently running to get better context when logging
    //
    // each log must have a reason, `generic` can be used if no reason makes sense
    //
    // in places where there is no context available, there's a global one can that
    // be retrieved with gcx() for logging
    //
    // all log functions will use fmt::format() internally, so they can be used
    // like:
    //
    //    cx.log(context::generic, "eat more {}", "potatoes");
    //
    class context {
    public:
        // reason for a log or bailing out
        //
        enum reason {
            // generic
            generic,

            // a configuration action
            conf,

            // something was bypassed because it was already done
            bypass,

            // something was done because the --redownload option was set
            redownload,

            // something was done because the --rebuild option was set
            rebuild,

            // something was done because the --reextract option was set
            reextract,

            // something was done in case of interruption or because something
            // was interrupted
            interruption,

            // command line of a process
            cmd,

            // output of a process
            std_out,
            std_err,

            // a filesystem action
            fs,

            // a network action
            net,
        };

        // level of a log entry, `dump` should only be used for really verbose
        // stuff that shouldn't be very useful, like curl's debugging logs
        //
        enum class level {
            dump = 1,
            trace,
            debug,
            info,
            warning,
            error,
        };

        // returns the global context, used by gcx() below
        //
        static const context* global();

        // whether logs of this level are enabled; this normally doesn't need to be
        // called because the logging functions will discard entries for levels that
        // are not enabled, but it can be used for log strings that expensive to
        // create
        //
        // since there are two log levels (one for console, one for file), enabled()
        // will return true if the given level is enabled for at least one of them
        //
        static bool enabled(level lv);

        // sets the output file for logs
        //
        static void set_log_file(const fs::path& p);

        // closes the output file for logs, see release_command::check_clean_prefix()
        //
        static void close_log_file();

        // creates a context for a task; the global context has no name
        //
        context(std::string task_name);

        // sets the tool that's currently running, may be null if there isn't one;
        // log entries will have the name of the tool if one is set
        //
        void set_tool(tool* t);

        // logs a simple string with the given level
        //
        void log_string(reason r, level lv, std::string_view s) const;

        // logs a formatted string with the given level
        //
        template <class... Args>
        void log(reason r, level lv, const char* f, Args&&... args) const
        {
            do_log(false, r, lv, f, std::forward<Args>(args)...);
        }

        // logs a formatted string with the dump level
        //
        template <class... Args>
        void dump(reason r, const char* f, Args&&... args) const
        {
            do_log(false, r, level::dump, f, std::forward<Args>(args)...);
        }

        // logs a formatted string with the trace level
        //
        template <class... Args>
        void trace(reason r, const char* f, Args&&... args) const
        {
            do_log(false, r, level::trace, f, std::forward<Args>(args)...);
        }

        // logs a formatted string with the debug level
        //
        template <class... Args>
        void debug(reason r, const char* f, Args&&... args) const
        {
            do_log(false, r, level::debug, f, std::forward<Args>(args)...);
        }

        // logs a formatted string with the info level
        //
        template <class... Args>
        void info(reason r, const char* f, Args&&... args) const
        {
            do_log(false, r, level::info, f, std::forward<Args>(args)...);
        }

        // logs a formatted string with the warning level
        //
        template <class... Args>
        void warning(reason r, const char* f, Args&&... args) const
        {
            do_log(false, r, level::warning, f, std::forward<Args>(args)...);
        }

        // logs a formatted string with the error level
        //
        template <class... Args>
        void error(reason r, const char* f, Args&&... args) const
        {
            do_log(false, r, level::error, f, std::forward<Args>(args)...);
        }

        // logs a formatted string with the error level and throws a bailed
        // exception, which will exit mob as quickly as it can, interrupting all
        // tasks
        //
        template <class... Args>
        [[noreturn]] void bail_out(reason r, const char* f, Args&&... args) const
        {
            do_log(true, r, level::error, f, std::forward<Args>(args)...);
        }

    private:
        // current task, may be empty
        std::string task_;

        // current tool, may be null
        const tool* tool_;

        // all logs above end up in here; if `bail` is true, this will throw a
        // bailed exception after logging
        //
        template <class... Args>
        void do_log(bool bail, reason r, level lv, const char* f, Args&&... args) const
        {
            // discard log if it's not enabled and it's not bailing out
            if (!bail && !enabled(lv))
                return;

            try {
                // formatting string
                const std::string s =
                    fmt::format(f, details::converter<std::decay_t<Args>>::convert(
                                       std::forward<Args>(args))...);

                do_log_impl(bail, r, lv, s);
            }
            catch (std::exception&) {
                // this is typically a bad format string, but there's not a lot
                // that can be done except logging to stderr and asserting

                // try to display the format string, but the console is in utf16
                // mode and the string is utf8, so it would have to be converted,
                // which could also fail
                //
                // since pretty much all format strings are ascii anyway, just do
                // a ghetto conversion and hope it gives enough info
                std::wstring s;

                const char* p = f;
                while (*p) {
                    s += (wchar_t)*p;
                    ++p;
                }

                std::wcerr << "bad format string '" << s << "'\n";

                if (IsDebuggerPresent())
                    DebugBreak();
            }
        }

        // all the calls above end up here; calls make_log_string() to get the full
        // log line calls emit_log() with it; throws after if `bail` is true
        //
        void do_log_impl(bool bail, reason r, level lv, std::string_view s) const;

        // formats the log line: adds the timestamp, task name and tool name, if
        // any
        //
        std::string_view make_log_string(reason r, level lv, std::string_view s) const;

        // writes the given string to the console and the log file, and keeps all
        // errors and warnings in global lists so they can be dumped just before mob
        // exits
        //
        void emit_log(level lv, std::string_view s) const;
    };

    // global context, convenience
    //
    inline const context& gcx()
    {
        return *context::global();
    }

    // called in main() just before mob exits, dumps all errors and warnings seen
    // during the build if the console log level was high enough
    //
    void dump_logs();

}  // namespace mob

#pragma once

#include "assert.h"

namespace mob {

    enum class encodings {
        dont_know = 0,
        utf8,
        utf16,
        acp,  // active code page
        oem   // oem code page
    };

    // replaces all instances of `from` by `to`, returns a copy
    //
    std::string replace_all(std::string s, const std::string& from,
                            const std::string& to);

    // concatenates all elements of `v`, separated by `sep`
    //
    template <class T, class Sep, class To = T>
    auto join(const std::vector<T>& v, const Sep& sep, To prefix = {})
    {
        bool first = true;

        for (auto&& e : v) {
            if (!first)
                prefix += sep;

            prefix += e;
            first = false;
        }

        return prefix;
    }

    // splits the given string on any character in `seps`
    //
    std::vector<std::string> split(const std::string& s, const std::string& seps);

    // splits the given string on any character in `seps` that are not between
    // double quotes
    //
    std::vector<std::string> split_quoted(const std::string& s,
                                          const std::string& seps);

    // adds enough of character `c` to the end of string `s` so its length is `n`
    // character
    //
    std::string pad_right(std::string s, std::size_t n, char c = ' ');

    // adds enough of character `c` to the start of string `s` so its length is `n`
    // character
    //
    std::string pad_left(std::string s, std::size_t n, char c = ' ');

    // removes any character found in `what` from both start and end of string;
    // in-place
    //
    void trim(std::string& s, std::string_view what = " \t\r\n");
    void trim(std::wstring& s, std::wstring_view what = L" \t\r\n");

    // removes any character found in `what` from both start and end of string;
    // returns a copy
    //
    std::string trim_copy(std::string_view s, std::string_view what = " \t\r\n");
    std::wstring trim_copy(std::wstring_view s, std::wstring_view what = L" \t\r\n");

    // formats a vector of pairs into two columns, putting `indent` spaces at the
    // start of each line and `spacing` spaces between the columns
    //
    std::string table(const std::vector<std::pair<std::string, std::string>>& v,
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
        const char* const end   = s.data() + s.size();

        const char* start = begin;
        const char* p     = begin;

        for (;;) {
            MOB_ASSERT(p && p >= begin && p <= end);
            MOB_ASSERT(start && start >= begin && start <= end);

            if (p == end || *p == '\n' || *p == '\r') {
                // end of line or string

                if (p != start) {
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
            else {
                ++p;
            }
        }
    }

    // an array of bytes using the specified encoding that can be parsed to call a
    // function with every line
    //
    // the output of a process is stored in an encoded_buffer and next_utf8_lines()
    // is called to process every line in it, avoiding copies or memory allocation,
    // except for conversions to utf8 when necessary
    //
    // if the encoding is dont_know, the buffer is basically interpreted as ascii
    // for checking newlines and the bytes are given as-is to the callback
    //
    class encoded_buffer {
    public:
        // a buffer using the given encoding and starting bytes
        //
        encoded_buffer(encodings e = encodings::dont_know, std::string bytes = {});

        // copies bytes to the internal buffer
        //
        void add(std::string_view bytes);

        // returns a copy of the internal buffer as utf8
        //
        std::string utf8_string() const;

        // calls `f()` with a utf8 string for every non-empty line in the buffer;
        // remembers the final offset when next_utf8_lines() was last called so
        // lines are only processed once
        //
        // if `finished` is false, it's assumed that more bytes will arrive
        // eventually, so the bytes after the last newline in the buffer are not
        // considered a line
        //
        // if `finished` is true, it's assumed that the output is complete and that
        // the final bytes before the end of the buffer are considered a valid line
        //
        template <class F>
        void next_utf8_lines(bool finished, F&& f)
        {
            // every case is the same, except for conversions:
            //   1) get the next, non-empty line
            //   2) if the line is empty, there wasn't any, so break
            //   3) convert to utf8 if needed and call f()

            for (;;) {
                switch (e_) {
                case encodings::utf16: {
                    std::wstring_view utf16 =
                        next_line<wchar_t>(finished, bytes_, last_);

                    if (utf16.empty())
                        return;

                    f(utf16_to_utf8(utf16));
                    break;
                }

                case encodings::acp:
                case encodings::oem: {
                    std::string_view cp = next_line<char>(finished, bytes_, last_);

                    if (cp.empty())
                        return;

                    f(bytes_to_utf8(e_, cp));
                    break;
                }

                case encodings::utf8:
                case encodings::dont_know:
                default: {
                    std::string_view utf8 = next_line<char>(finished, bytes_, last_);

                    if (utf8.empty())
                        return;

                    f(std::string(utf8));
                    break;
                }
                }
            }
        }

    private:
        // encoding of the buffer
        encodings e_;

        // internal buffer
        std::string bytes_;

        // offset of the last newline found the last time next_utf8_lines() was
        // called
        std::size_t last_;

        // looks for the next newline character after last_ and returns a
        // string_view of the data between the two; empty lines are ignored,
        // handles both lf and crlf the same
        //
        // this is a static function, but it's always given bytes_ and last_ as
        // arguments
        //
        template <class CharT>
        static std::basic_string_view<CharT>
        next_line(bool finished, const std::string_view bytes, std::size_t& byte_offset)
        {
            // number of available bytes in the buffer
            //
            // for utf16, it's possible (but unlikely) that the buffer has an odd
            // number of bytes if not all the output was flushed, so don't check the
            // last stray byte
            //
            // this doesn't handle stray bytes for other encodings, but they use
            // single bytes for cr and lf, so it's fine
            //
            const std::size_t size = [&] {
                if constexpr (sizeof(CharT) == 2) {
                    if ((bytes.size() & 1) == 1)
                        return bytes.size() - 1;
                }

                return bytes.size();
            }();

            // position just past where the last newline was found
            const CharT* start =
                reinterpret_cast<const CharT*>(bytes.data() + byte_offset);

            // end of the buffer
            const CharT* const end =
                reinterpret_cast<const CharT*>(bytes.data() + size);

            // current character being checked
            const CharT* p = start;

            // line that was found, or empty if none is available
            std::basic_string_view<CharT> line;

            // looking for a non-empty line
            while (p != end) {
                if (*p == CharT('\n') || *p == CharT('\r')) {
                    line = {start, static_cast<std::size_t>(p - start)};

                    // skip newline characters from this point
                    while (p != end && (*p == CharT('\n') || *p == CharT('\r')))
                        ++p;

                    // line is not empty, take it
                    if (!line.empty())
                        break;

                    // line can be empty for something like \n\n, continue looking
                    // for a non-empty line if that's the case
                    start = p;
                }
                else {
                    ++p;
                }
            }

            // if the line is empty but `finished` is true, make sure the last
            // line in the buffer is handled
            //
            if (line.empty()) {
                if (finished) {
                    // the line is from past the last newline to the end of the
                    // buffer; this may be empty if the buffer actually ends with
                    // a newline, which is fine
                    line = {reinterpret_cast<const CharT*>(bytes.data() + byte_offset),
                            size - byte_offset};

                    // tell the caller that whole thing has been processed
                    byte_offset = bytes.size();
                }
            }
            else {
                // a non-empty line was found, update the offset to be past the
                // newline character(s)

                byte_offset = static_cast<std::size_t>(
                    reinterpret_cast<const char*>(p) - bytes.data());

                MOB_ASSERT(byte_offset <= bytes.size());
            }

            return line;
        }
    };

}  // namespace mob

#pragma once

namespace mob {

    // sets the current console color in the constructor, restores it in the
    // destructor
    //
    class console_color {
    public:
        enum colors { white, grey, yellow, red };

        // no-op
        //
        console_color();

        // sets the given color in the console
        //
        console_color(colors c);

        // resets the color
        //
        ~console_color();

        // non-copyable
        console_color(const console_color&)            = delete;
        console_color& operator=(const console_color&) = delete;

    private:
        // true when the color was changed and must be reset in the destructor
        bool reset_;

        // old color
        WORD old_atts_;
    };

    // a stream that accepts utf8 strings and writes them to stdout/stderr
    //
    // if stdout/stderr is a console, converts to utf16 and outputs; if it's
    // redirected, outputs utf8 directly
    //
    // thread-safe, no interleaving
    //
    class u8stream {
    public:
        // use stderr when err is true, stdout otherwise
        //
        u8stream(bool err) : err_(err) {}

        // outputs arguments without a newline
        //
        template <class... Args>
        u8stream& operator<<(Args&&... args)
        {
            std::ostringstream oss;
            ((oss << std::forward<Args>(args)), ...);

            do_output(oss.str());

            return *this;
        }

        // outputs given string followed by a newline
        //
        void write_ln(std::string_view utf8);

    private:
        // whether this is stdout or stderr
        bool err_;

        // outputs the given utf8 string
        //
        void do_output(const std::string& utf8);
    };

    // stdout
    extern u8stream u8cout;

    // stderr
    extern u8stream u8cerr;

    // called from main(), changes the standard output to utf-16
    //
    void set_std_streams();

    // the global mutex used to avoid interleaving
    //
    std::mutex& global_output_mutex();

    enum class yn { no = 0, yes, cancelled };

    // asks the user for y/n
    //
    yn ask_yes_no(const std::string& text, yn def);

    // see https://github.com/isanae/mob/issues/4
    //
    // this saves the current console font in the constructor and restores it in
    // the destructor if it changed
    //
    class font_restorer {
    public:
        font_restorer();
        ~font_restorer();

        void restore();

    private:
        CONSOLE_FONT_INFOEX old_;
        bool restore_;
    };

}  // namespace mob

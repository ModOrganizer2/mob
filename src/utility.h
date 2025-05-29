#pragma once

#include "utility/algo.h"
#include "utility/enum.h"
#include "utility/fs.h"
#include "utility/io.h"
#include "utility/string.h"
#include "utility/threading.h"

namespace mob {

    // time since mob started
    //
    std::chrono::nanoseconds timestamp();

    // exception thrown when a task failed
    //
    class bailed {
    public:
        bailed(std::string s = {}) : s_(std::move(s)) {}

        const char* what() const { return s_.c_str(); }

    private:
        std::string s_;
    };

    // executes the given function in the destructor
    //
    template <class F>
    class guard {
    public:
        guard(F f) : f_(f) {}

        ~guard() { f_(); }

    private:
        F f_;
    };

    enum class arch {
        x86 = 1,
        x64,
        dont_care,

        def = x64
    };

    enum class config { debug, relwithdebinfo, release };

    class url;

    // returns "mob x.y"
    //
    std::string mob_version();

}  // namespace mob

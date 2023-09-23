#pragma once

namespace mob {

#define MOB_WIDEN2(x) L##x
#define MOB_WIDEN(x) MOB_WIDEN2(x)
#define MOB_FILE_UTF16 MOB_WIDEN(__FILE__)

#define MOB_ASSERT(x, ...)                                                             \
    mob_assert(x, __VA_ARGS__, #x, MOB_FILE_UTF16, __LINE__, __FUNCSIG__);

    void mob_assertion_failed(const char* message, const char* exp, const wchar_t* file,
                              int line, const char* func);

    template <class X>
    inline void mob_assert(X&& x, const char* message, const char* exp,
                           const wchar_t* file, int line, const char* func)
    {
        if (!(x))
            mob_assertion_failed(message, exp, file, line, func);
    }

    template <class X>
    inline void mob_assert(X&& x, const char* exp, const wchar_t* file, int line,
                           const char* func)
    {
        if (!(x))
            mob_assertion_failed(nullptr, exp, file, line, func);
    }

}  // namespace mob

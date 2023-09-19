#include "pch.h"
#include "env.h"
#include "../tools/tools.h"
#include "../utility.h"
#include "conf.h"
#include "context.h"
#include "op.h"
#include "process.h"

namespace mob {

    // retrieves the Visual Studio environment variables for the given architecture;
    // this is pretty expensive, so it's called on demand and only once, and is
    // stored as a static variable in vs_x86() and vs_x64() below
    //
    env get_vcvars_env(arch a)
    {
        // translate arch to the string needed by vcvars
        std::string arch_s;

        switch (a) {
        case arch::x86:
            arch_s = "x86";
            break;

        case arch::x64:
            arch_s = "amd64";
            break;

        case arch::dont_care:
        default:
            gcx().bail_out(context::generic, "get_vcvars_env: bad arch");
        }

        gcx().trace(context::generic, "looking for vcvars for {}", arch_s);

        // the only way to get these variables is to
        //   1) run vcvars in a cmd instance,
        //   2) call `set`, which outputs all the variables to stdout, and
        //   3) parse it
        //
        // the process class doesn't really have a good way of dealing with this
        // and it's not worth adding all this crap to it just for vcvars, so most
        // of this is done manually

        // stdout will be redirected to this
        const fs::path tmp = make_temp_file();

        // runs `"vcvarsall.bat" amd64 && set > temp_file`
        const std::string cmd = "\"" + path_to_utf8(vs::vcvars()) + "\" " + arch_s +
                                " && set > \"" + path_to_utf8(tmp) + "\"";

        // cmd_unicode() is necessary so `set` outputs in utf16 instead of codepage
        process::raw(gcx(), cmd).cmd_unicode(true).run();

        gcx().trace(context::generic, "reading from {}", tmp);

        // reads the file, converting utf16 to utf8
        std::stringstream ss(op::read_text_file(gcx(), encodings::utf16, tmp));
        op::delete_file(gcx(), tmp);

        // `ss` contains all the variables in utf8

        env e;

        gcx().trace(context::generic, "parsing variables");

        for (;;) {
            std::string line;
            std::getline(ss, line);
            if (!ss)
                break;

            const auto sep = line.find('=');

            if (sep == std::string::npos)
                continue;

            std::string name  = line.substr(0, sep);
            std::string value = line.substr(sep + 1);

            gcx().trace(context::generic, "{} = {}", name, value);
            e.set(std::move(name), std::move(value));
        }

        return e;
    }

    env env::vs_x86()
    {
        static env e = get_vcvars_env(arch::x86);
        return e;
    }

    env env::vs_x64()
    {
        static env e = get_vcvars_env(arch::x64);
        return e;
    }

    env env::vs(arch a)
    {
        switch (a) {
        case arch::x86:
            return vs_x86();

        case arch::x64:
            return vs_x64();

        case arch::dont_care:
            return {};

        default:
            gcx().bail_out(context::generic, "bad arch for env");
        }
    }

    env::env() : own_(false)
    {
        // empty env, does not own
    }

    env::env(const env& e) : data_(e.data_), own_(false)
    {
        // copy data, does not own
    }

    env::env(env&& e) : data_(std::move(e.data_)), own_(e.own_)
    {
        // move data, owns if `e` did
    }

    env& env::operator=(const env& e)
    {
        // copy data, does not own
        data_ = e.data_;
        own_  = false;
        return *this;
    }

    env& env::operator=(env&& e)
    {
        // copy data, owns if `e` did
        data_ = std::move(e.data_);
        own_  = e.own_;
        return *this;
    }

    env& env::append_path(const fs::path& p)
    {
        append_path(std::vector<fs::path>{p});
        return *this;
    }

    env& env::prepend_path(const fs::path& p)
    {
        prepend_path(std::vector<fs::path>{p});
        return *this;
    }

    env& env::prepend_path(const std::vector<fs::path>& v)
    {
        change_path(v, prepend);
        return *this;
    }

    env& env::append_path(const std::vector<fs::path>& v)
    {
        change_path(v, append);
        return *this;
    }

    env& env::change_path(const std::vector<fs::path>& v, flags f)
    {
        copy_for_write();

        std::wstring path;

        switch (f) {
        case replace: {
            // convert to utf16 strings, join with ;
            const auto strings = mob::map(v, [&](auto&& p) {
                return p.native();
            });

            path = join(strings, L";");

            break;
        }

        case append: {
            auto current = find(L"PATH");
            if (current)
                path = *current;

            // append all paths as utf16 strings to the current value, if any
            for (auto&& p : v) {
                if (!path.empty())
                    path += L";";

                path += p.native();
            }

            break;
        }

        case prepend: {
            auto current = find(L"PATH");
            if (current)
                path = *current;

            // prepend all paths as utf16 strings to the current value, if any
            for (auto&& p : v) {
                if (!path.empty())
                    path = L";" + path;

                path = p.native() + path;
            }

            break;
        }
        }

        set(L"PATH", path, replace);

        return *this;
    }

    env& env::set(std::string_view k, std::string_view v, flags f)
    {
        copy_for_write();
        set_impl(utf8_to_utf16(k), utf8_to_utf16(v), f);
        return *this;
    }

    env& env::set(std::wstring k, std::wstring v, flags f)
    {
        copy_for_write();
        set_impl(std::move(k), std::move(v), f);
        return *this;
    }

    void env::set_impl(std::wstring k, std::wstring v, flags f)
    {
        auto current = find(k);

        if (!current) {
            data_->vars.emplace(std::move(k), std::move(v));
            return;
        }

        switch (f) {
        case replace:
            *current = std::move(v);
            break;

        case append:
            *current += v;
            break;

        case prepend:
            *current = v + *current;
            break;
        }
    }

    std::string env::get(std::string_view k) const
    {
        if (!data_)
            return {};

        auto current = find(utf8_to_utf16(k));
        if (!current)
            return {};

        return utf16_to_utf8(*current);
    }

    env::map env::get_map() const
    {
        if (!data_)
            return {};

        std::scoped_lock lock(data_->m);
        return data_->vars;
    }

    void env::create_sys() const
    {
        // CreateProcess() wants a string where every key=value is separated by a
        // null and also terminated by a null, so there are two null characters at
        // the end

        data_->sys.clear();

        for (auto&& v : data_->vars) {
            data_->sys += v.first + L"=" + v.second;
            data_->sys.append(1, L'\0');
        }

        data_->sys.append(1, L'\0');
    }

    std::wstring* env::find(std::wstring_view name)
    {
        return const_cast<std::wstring*>(std::as_const(*this).find(name));
    }

    const std::wstring* env::find(std::wstring_view name) const
    {
        if (!data_)
            return {};

        for (auto itor = data_->vars.begin(); itor != data_->vars.end(); ++itor) {
            if (_wcsicmp(itor->first.c_str(), name.data()) == 0)
                return &itor->second;
        }

        return {};
    }

    void* env::get_unicode_pointers() const
    {
        if (!data_ || data_->vars.empty())
            return nullptr;

        // create string if it doesn't exist
        {
            std::scoped_lock lock(data_->m);
            if (data_->sys.empty())
                create_sys();
        }

        return (void*)data_->sys.c_str();
    }

    void env::copy_for_write()
    {
        if (own_) {
            // this is called every time something is about to change; if this
            // instance already owns the data, the sys strings must still be cleared
            // out so they're recreated if get_unicode_pointers() is every called
            if (data_)
                data_->sys.clear();

            return;
        }

        if (data_) {
            // remember the shared data
            auto shared = data_;

            // create a new owned instance
            data_.reset(new data);

            // copying
            std::scoped_lock lock(shared->m);
            data_->vars = shared->vars;
        }
        else {
            // creating own, empty data
            data_.reset(new data);
        }

        // this instance owns the data
        own_ = true;
    }

    // mob's environment variables are only retrieved once and are kept in sync
    // after that; this must also be thread-safe
    static std::mutex g_sys_env_mutex;
    static env g_sys_env;
    static bool g_sys_env_inited;

    env this_env::get()
    {
        std::scoped_lock lock(g_sys_env_mutex);

        if (g_sys_env_inited) {
            // already done
            return g_sys_env;
        }

        // first time, get the variables from the system

        auto free = [](wchar_t* p) {
            FreeEnvironmentStringsW(p);
        };

        auto env_block =
            std::unique_ptr<wchar_t, decltype(free)>{GetEnvironmentStringsW(), free};

        // GetEnvironmentStringsW() returns a string where each variable=value
        // is separated by a null character

        for (const wchar_t* name = env_block.get(); *name != L'\0';) {
            // equal sign
            const wchar_t* equal = std::wcschr(name, '=');

            // key
            std::wstring key(name, static_cast<std::size_t>(equal - name));

            // value
            const wchar_t* value_start = equal + 1;
            std::wstring value(value_start);

            // the strings contain all sorts of weird stuff, like variables to
            // keep track of the current directory, those start with an equal sign,
            // so just ignore them
            if (!key.empty())
                g_sys_env.set(key, value);

            // next string is one past end of value to account for null byte
            name = value_start + value.length() + 1;
        }

        g_sys_env_inited = true;

        return g_sys_env;
    }

    void this_env::set(const std::string& k, const std::string& v, env::flags f)
    {
        const std::wstring wk = utf8_to_utf16(k);
        std::wstring wv       = utf8_to_utf16(v);

        switch (f) {
        case env::replace: {
            ::SetEnvironmentVariableW(wk.c_str(), wv.c_str());
            break;
        }

        case env::append: {
            const std::wstring current = get_impl(k).value_or(L"");
            wv                         = current + wv;
            ::SetEnvironmentVariableW(wk.c_str(), wv.c_str());
            break;
        }

        case env::prepend: {
            const std::wstring current = get_impl(k).value_or(L"");
            wv                         = wv + current;
            ::SetEnvironmentVariableW(wk.c_str(), wv.c_str());
            break;
        }
        }

        // keep in sync
        {
            std::scoped_lock lock(g_sys_env_mutex);
            if (g_sys_env_inited)
                g_sys_env.set(utf8_to_utf16(k), wv);
        }
    }

    void this_env::prepend_to_path(const fs::path& p)
    {
        gcx().trace(context::generic, "prepending to PATH: {}", p);
        set("PATH", path_to_utf8(p) + ";", env::prepend);
    }

    void this_env::append_to_path(const fs::path& p)
    {
        gcx().trace(context::generic, "appending to PATH: {}", p);
        set("PATH", ";" + path_to_utf8(p), env::append);
    }

    std::string this_env::get(const std::string& name)
    {
        auto v = get_impl(name);
        if (!v) {
            gcx().bail_out(context::generic, "environment variable {} doesn't exist",
                           name);
        }

        return utf16_to_utf8(*v);
    }

    std::optional<std::string> this_env::get_opt(const std::string& name)
    {
        auto v = get_impl(name);
        if (v)
            return utf16_to_utf8(*v);
        else
            return {};
    }

    std::optional<std::wstring> this_env::get_impl(const std::string& k)
    {
        const std::wstring wk = utf8_to_utf16(k);

        const std::size_t buffer_size = GetEnvironmentVariableW(wk.c_str(), nullptr, 0);

        if (buffer_size == 0)
            return {};

        auto buffer = std::make_unique<wchar_t[]>(buffer_size + 1);
        std::fill(buffer.get(), buffer.get() + buffer_size + 1, 0);

        const std::size_t written = GetEnvironmentVariableW(
            wk.c_str(), buffer.get(), static_cast<DWORD>(buffer_size));

        if (written == 0)
            return {};

        MOB_ASSERT((written + 1) == buffer_size);

        return std::wstring(buffer.get(), buffer.get() + written);
    }

}  // namespace mob

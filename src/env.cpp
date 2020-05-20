#include "pch.h"
#include "env.h"
#include "conf.h"
#include "process.h"
#include "op.h"
#include "context.h"
#include "tools/tools.h"

namespace mob
{

env get_vcvars_env(arch a)
{
	std::string arch_s;

	switch (a)
	{
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

	const fs::path tmp = make_temp_file();

	// "vcvarsall.bat" amd64 && set > temp_file
	const std::string cmd =
		"\"" + path_to_utf8(vs::vcvars()) + "\" " + arch_s +
		" && set > \"" + path_to_utf8(tmp) + "\"";

	process::raw(gcx(), cmd)
		.cmd_unicode(true)
		.run();

	gcx().trace(context::generic, "reading from {}", tmp);

	std::stringstream ss(op::read_text_file(gcx(), encodings::utf16, tmp));
	op::delete_file(gcx(), tmp);

	env e;

	gcx().trace(context::generic, "parsing variables");

	for (;;)
	{
		std::string line;
		std::getline(ss, line);
		if (!ss)
			break;

		const auto sep = line.find('=');

		if (sep == std::string::npos)
			continue;

		std::string name = line.substr(0, sep);
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
	switch (a)
	{
		case arch::x86:
			return vs_x86();

		case arch::x64:
			return vs_x64();

		case arch::dont_care:
			return {};

		default:
			bail_out("bad arch for env");
	}
}

env& env::append_path(const fs::path& p)
{
	return append_path(std::vector<fs::path>{p});
}

env& env::append_path(const std::vector<fs::path>& v)
{
	std::string path;

	auto itor = find("PATH");
	if (itor != vars_.end())
		path = itor->second;

	for (auto&& p : v)
	{
		if (!path.empty())
			path += ";";

		path += path_to_utf8(p);
	}

	set("PATH", path, replace);

	return *this;
}

env& env::set(std::string k, std::string v, flags f)
{
	auto itor = find(k);

	if (itor == vars_.end())
	{
		vars_[k] = v;
		return *this;
	}

	switch (f)
	{
		case replace:
			vars_[itor->first] = v;
			break;

		case append:
			vars_[itor->first] += v;
			break;

		case prepend:
			vars_[itor->first] = v + vars_[itor->first];
			break;
	}

	return *this;
}

std::string env::get(const std::string& k) const
{
	auto itor = find(k);
	if (itor == vars_.end())
		return {};

	return itor->second;
}

void env::set_from(const env& e)
{
	for (auto&& v : e.vars_)
		set(v.first, v.second, replace);
}

void env::create() const
{
	sys_.clear();

	for (auto&& v : vars_)
	{
		sys_ += utf8_to_utf16(v.first + "=" + v.second);
		sys_.append(1, L'\0');
	}

	sys_.append(1, L'\0');
}

env::map::const_iterator env::find(const std::string& name) const
{
	for (auto itor=vars_.begin(); itor!=vars_.end(); ++itor)
	{
		if (_stricmp(itor->first.c_str(), name.c_str()) == 0)
			return itor;
	}

	return vars_.end();
}

void* env::get_unicode_pointers() const
{
	if (vars_.empty())
		return nullptr;

	if (sys_.empty())
		create();

	return (void*)sys_.c_str();
}



static std::mutex g_sys_env_mutex;
static env g_sys_env;
static bool g_sys_env_inited;

env this_env::get()
{
	std::scoped_lock lock(g_sys_env_mutex);

	if (g_sys_env_inited)
		return g_sys_env;

	auto free = [](wchar_t* p) { FreeEnvironmentStringsW(p); };

	auto env_block = std::unique_ptr<wchar_t, decltype(free)>{
		GetEnvironmentStringsW(), free};

	for (const wchar_t* name = env_block.get(); *name != L'\0'; )
	{
		const wchar_t* equal = std::wcschr(name, '=');
		std::wstring key(name, static_cast<std::size_t>(equal - name));

		const wchar_t* pValue = equal + 1;
		std::wstring value(pValue);

		if (!key.empty())
			g_sys_env.set(utf16_to_utf8(key), utf16_to_utf8(value));

		name = pValue + value.length() + 1;
	}

	g_sys_env_inited = true;

	return g_sys_env;
}

void this_env::set(const std::string& k, const std::string& v, env::flags f)
{
	const std::wstring wk = utf8_to_utf16(k);
	std::wstring wv = utf8_to_utf16(v);

	switch (f)
	{
		case env::replace:
		{
			::SetEnvironmentVariableW(wk.c_str(), wv.c_str());
			break;
		}

		case env::append:
		{
			const std::wstring current = get_impl(k).value_or(L"");
			wv = current + wv;
			::SetEnvironmentVariableW(wk.c_str(), wv.c_str());
			break;
		}

		case env::prepend:
		{
			const std::wstring current = get_impl(k).value_or(L"");
			wv = wv + current;
			::SetEnvironmentVariableW(wk.c_str(), wv.c_str());
			break;
		}
	}

	{
		std::scoped_lock lock(g_sys_env_mutex);
		if (g_sys_env_inited)
			g_sys_env.set(k, utf16_to_utf8(wv));
	}
}

void this_env::prepend_to_path(const fs::path& p)
{
	gcx().trace(context::generic, "prepending to PATH: {}", p);
	set("PATH", path_to_utf8(p) + ";", env::prepend);
}

std::string this_env::get(const std::string& name)
{
	auto v = get_impl(name);
	if (!v)
		bail_out("environment variable {} doesn't exist", name);

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

	const std::size_t buffer_size = GetEnvironmentVariableW(
		wk.c_str(), nullptr, 0);

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

}  // namespace

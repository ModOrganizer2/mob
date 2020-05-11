#include "pch.h"
#include "env.h"
#include "conf.h"
#include "process.h"
#include "op.h"
#include "context.h"

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
	std::string cmd =
		"\"" + tools::vs::vcvars().string() + "\" " + arch_s +
		" && set > \"" + tmp.string() + "\"";

	process::raw(gcx(), cmd)
		.run();

	gcx().trace(context::generic, "reading from {}", tmp);

	std::stringstream ss(op::read_text_file(gcx(), tmp));
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

		path += p.string();
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
	for (auto&& v : vars_)
	{
		string_ += v.first + "=" + v.second;
		string_.append(1, '\0');
	}

	string_.append(1, '\0');
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

void* env::get_pointers() const
{
	if (vars_.empty())
		return nullptr;

	if (string_.empty())
		create();

	return (void*)string_.c_str();
}



void this_env::set(const std::string& k, const std::string& v, env::flags f)
{
	switch (f)
	{
		case env::replace:
			::SetEnvironmentVariableA(k.c_str(), v.c_str());
			break;

		case env::append:
			::SetEnvironmentVariableA(k.c_str(), (get(k) + v).c_str());
			break;

		case env::prepend:
			::SetEnvironmentVariableA(k.c_str(), (v + get(k)).c_str());
			break;
	}
}

void this_env::prepend_to_path(const fs::path& p)
{
	gcx().trace(context::generic, "prepending to PATH: {}", p);
	set("PATH", p.string() + ";", env::prepend);
}

std::string this_env::get(const std::string& name)
{
	const std::size_t buffer_size = GetEnvironmentVariableA(
		name.c_str(), nullptr, 0);

	if (buffer_size == 0)
		bail_out("environment variable {} doesn't exist", name);

	auto buffer = std::make_unique<char[]>(buffer_size + 1);
	std::fill(buffer.get(), buffer.get() + buffer_size + 1, 0);

	GetEnvironmentVariableA(
		name.c_str(), buffer.get(), static_cast<DWORD>(buffer_size));

	return buffer.get();
}

env this_env::get()
{
	env e;

	auto free = [](char* p) { FreeEnvironmentStrings(p); };

	auto env_block = std::unique_ptr<char, decltype(free)>{
		GetEnvironmentStrings(), free};

	for (const char* name = env_block.get(); *name != '\0'; )
	{
		const char* equal = std::strchr(name, '=');
		std::string key(name, static_cast<std::size_t>(equal - name));

		const char* pValue = equal + 1;
		std::string value(pValue);

		if (!key.empty())
			e.set(key, value);

		name = pValue + value.length() + 1;
	}

	return e;
}

}  // namespace

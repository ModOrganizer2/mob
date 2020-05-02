#include "pch.h"
#include "utility.h"
#include "conf.h"
#include "op.h"
#include "net.h"

namespace builder
{

struct color
{
	color(int r, int g, int b)
	{
		std::cout << "\033[38;2;" << r << ";" << g << ";" << b << "m";
	}

	~color()
	{
		std::cout << "\033[39m\033[49m";
	}

	color(const color&) = delete;
	color& operator=(const color&) = delete;
};

std::unique_ptr<color> color_for_level(level lv)
{
	switch (lv)
	{
		case level::error:
		case level::bail:
			return std::make_unique<color>(240, 50, 50);

		case level::warning:
			return std::make_unique<color>(240, 240, 50);

		case level::debug:
			return std::make_unique<color>(150, 150, 150);

		case level::info:
		default:
			return {};
	}
}


std::string error_message(DWORD e)
{
	return std::error_code(
		static_cast<int>(e), std::system_category()).message();
}


std::vector<std::string> g_errors, g_warnings;
std::mutex g_log_mutex;

void out(level lv, const std::string& s)
{
	if (lv == level::debug && !conf::verbose())
		return;

	{
		std::scoped_lock lock(g_log_mutex);
		auto c = color_for_level(lv);

		std::cout << s << "\n";

		if (lv == level::error || lv == level::bail)
			g_errors.push_back(s);
		else if (lv == level::warning)
			g_warnings.push_back(s);
	}

	if (lv == level::bail)
		throw bailed(s);
}

void out(level lv, const std::string& s, DWORD e)
{
	out(lv, s + ", " + error_message(e));
}

void out(level lv, const std::string& s, const std::error_code& e)
{
	out(lv, s + ", " + e.message());
}

void dump_logs()
{
	if (!g_warnings.empty())
	{
		auto c = color_for_level(level::warning);
		std::cout << "\n\nthere were warnings:\n";

		for (auto&& s : g_warnings)
			std::cout << " - " << s << "\n";
	}

	if (!g_errors.empty())
	{
		auto c = color_for_level(level::error);
		std::cout << "\n\nthere were errors:\n";

		for (auto&& s : g_errors)
			std::cout << " - " << s << "\n";
	}
}


std::string read_text_file(const fs::path& p)
{
	debug("reading " + p.string());

	std::string s;

	std::ifstream in(p);
	if (!in)
		bail_out("can't read from " + p.string() + "'");

	in.seekg(0, std::ios::end);
	s.resize(static_cast<std::size_t>(in.tellg()));
	in.seekg(0, std::ios::beg);
	in.read(&s[0], static_cast<std::streamsize>(s.size()));

	return s;
}

std::string replace_all(
	std::string s, const std::string& from, const std::string& to)
{
	for (;;)
	{
		const auto pos = s.find(from);
		if (pos == std::string::npos)
			break;

		s.replace(pos, from.size(), to);
	}

	return s;
}

std::string join(const std::vector<std::string>& v, const std::string& sep)
{
	std::string s;

	for (auto&& e : v)
	{
		if (!s.empty())
			s += sep;

		s += e;
	}

	return s;
}


std::string redir_nul()
{
	if (conf::verbose())
		return {};
	else
		return " > NUL";
}


const std::string& cmd::string() const
{
	return s_;
}

void cmd::add_arg(const std::string& k, const std::string& v, flags f)
{
	if ((f & quiet) && conf::verbose())
		return;

	if (k.empty() && v.empty())
		return;

	if (k.empty())
		s_ += " " + v;
	else if ((f & nospace) || k.back() == '=')
		s_ += " " + k + v;
	else
		s_ += " " + k + " " + v;
}

std::string cmd::arg_to_string(const char* s, bool force_quote)
{
	if (force_quote)
		return "\"" + std::string(s) + "\"";
	else
		return s;
}

std::string cmd::arg_to_string(const std::string& s, bool force_quote)
{
	if (force_quote)
		return "\"" + std::string(s) + "\"";
	else
		return s;
}

std::string cmd::arg_to_string(const fs::path& p, bool)
{
	return "\"" + p.string() + "\"";
}

std::string cmd::arg_to_string(const url& u, bool force_quote)
{
	if (force_quote)
		return "\"" + u.string() + "\"";
	else
		return u.string();
}


file_deleter::file_deleter(fs::path p)
	: p_(std::move(p)), delete_(true)
{
}

file_deleter::~file_deleter()
{
	if (delete_)
		delete_now();
}

void file_deleter::delete_now()
{
	if (fs::exists(p_))
		op::delete_file(p_);
}

void file_deleter::cancel()
{
	delete_ = false;
}


directory_deleter::directory_deleter(fs::path p)
	: p_(std::move(p)), delete_(true)
{
}

directory_deleter::~directory_deleter()
{
	if (delete_)
		delete_now();
}

void directory_deleter::delete_now()
{
	if (fs::exists(p_))
		op::delete_directory(p_);
}

void directory_deleter::cancel()
{
	delete_ = false;
}


static env g_env_x86, g_env_x64;


fs::path find_vcvars()
{
	const std::vector<std::string> editions =
	{
		"Preview", "Enterprise", "Professional", "Community"
	};

	for (auto&& edition : editions)
	{
		const auto p =
			paths::program_files_x86() /
			"Microsoft Visual Studio" /
			versions::vs_year() /
			edition / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat";

		if (fs::exists(p))
		{
			debug("found " + p.string());
			return p;
		}
	}

	bail_out("couldn't find visual studio");
}

env get_vcvars_env(arch a)
{
	debug("vcvars");

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
			bail_out("get_vcvars_env: bad arch");
	}

	const fs::path tmp = paths::temp_file();

	// "vcvarsall.bat" amd64 && set > temp_file
	const std::string cmd =
		"\"" + find_vcvars().string() + "\" " + arch_s + " && "
		"set > \"" + tmp.string() + "\"";

	process::raw(cmd).run();

	std::stringstream ss(read_text_file(tmp));
	op::delete_file(tmp);


	env e;

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

		e.set(std::move(name), std::move(value));
	}

	return e;
}

void vcvars()
{
	g_env_x86 = get_vcvars_env(arch::x86);
	g_env_x64 = get_vcvars_env(arch::x64);
}

env env::vs_x64()
{
	return g_env_x64;
}

env env::vs_x86()
{
	return g_env_x86;
}

env env::vs(arch a)
{
	switch (a)
	{
		case arch::x86:
			return g_env_x86;

		case arch::x64:
			return g_env_x64;

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

}	// namespace


namespace builder::current_env
{

void set(const std::string& k, const std::string& v, env::flags f)
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

std::string get(const std::string& name)
{
	const std::size_t buffer_size = GetEnvironmentVariableA(
		name.c_str(), nullptr, 0);

	if (buffer_size == 0)
		bail_out("environment variable " + name + " doesn't exist");

	auto buffer = std::make_unique<char[]>(buffer_size + 1);
	std::fill(buffer.get(), buffer.get() + buffer_size + 1, 0);

	GetEnvironmentVariableA(
		name.c_str(), buffer.get(), static_cast<DWORD>(buffer_size));

	return buffer.get();
}

env get()
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

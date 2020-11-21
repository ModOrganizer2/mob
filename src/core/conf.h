#pragma once

namespace mob
{

std::string default_ini_filename();

std::vector<fs::path> find_inis(
	bool auto_detect, const std::vector<std::string>& from_cl,
	bool verbose);

void init_options(
	const std::vector<fs::path>& inis, const std::vector<std::string>& opts);

bool verify_options();
void log_options();
void dump_available_options();

fs::path find_in_path(std::string_view exe);
fs::path make_temp_file();


namespace details
{
	std::string get_string(std::string_view section, std::string_view key);
	bool get_bool(std::string_view section, std::string_view key);
	int get_int(std::string_view section, std::string_view key);

	void set_string(
		std::string_view section, std::string_view key,
		std::string_view value);
}


template <class DefaultType>
class conf_section
{
public:
	DefaultType get(std::string_view key) const
	{
		return details::get_string(name_, key);
	}

	template <class T>
	T get(std::string_view key) const;

	template <>
	bool get<bool>(std::string_view key) const
	{
		return details::get_bool(name_, key);
	}

	template <>
	int get<int>(std::string_view key) const
	{
		return details::get_int(name_, key);
	}

	void set(std::string_view key, std::string_view value)
	{
		details::set_string(name_, key, value);
	}

protected:
	conf_section(std::string section_name)
		: name_(std::move(section_name))
	{
	}

private:
	std::string name_;
};


class conf_global : public conf_section<std::string>
{
public:
	conf_global();

	int output_log_level() const;
	void set_output_log_level(const std::string& s);

	int file_log_level() const;
	void set_file_log_level(const std::string& s);

	bool dry() const;
	void set_dry(std::string_view s);

	fs::path log_file() const { return get("log_file"); }

	bool redownload()   const { return get<bool>("redownload"); }
	bool reextract()    const { return get<bool>("reextract"); }
	bool reconfigure()  const { return get<bool>("reconfigure"); }
	bool rebuild()      const { return get<bool>("rebuild"); }
	bool clean()        const { return get<bool>("clean_task"); }
	bool fetch()        const { return get<bool>("fetch_task"); }
	bool build()        const { return get<bool>("build_task"); }

	bool ignore_uncommitted() const
	{
		return get<bool>("ignore_uncommitted");
	}
};

class conf_task
{
public:
	conf_task(std::vector<std::string> names);

	std::string get(std::string_view key) const;

	template <class T>
	T get(std::string_view key) const;

	template <>
	bool get<bool>(std::string_view key) const
	{
		return get_bool(key);
	}

private:
	std::vector<std::string> names_;

	bool get_bool(std::string_view name) const;
};

class conf_tools : public conf_section<fs::path>
{
public:
	conf_tools();
};

class conf_transifex : public conf_section<std::string>
{
public:
	conf_transifex();
};

class conf_versions : public conf_section<std::string>
{
public:
	conf_versions();
};

class conf_prebuilt : public conf_section<std::string>
{
public:
	conf_prebuilt();

	bool use_prebuilt(std::string_view task_name)
	{
		return get<bool>(task_name);
	}
};


class conf_paths : public conf_section<fs::path>
{
public:
	conf_paths();

#define VALUE(NAME) \
	fs::path NAME() const { return get(#NAME); }

	VALUE(third_party);
	VALUE(prefix);
	VALUE(cache);
	VALUE(patches);
	VALUE(licenses);
	VALUE(build);

	VALUE(install);
	VALUE(install_installer);
	VALUE(install_bin);
	VALUE(install_libs);
	VALUE(install_pdbs);

	VALUE(install_dlls);
	VALUE(install_loot);
	VALUE(install_plugins);
	VALUE(install_stylesheets);
	VALUE(install_licenses);
	VALUE(install_pythoncore);
	VALUE(install_translations);

	VALUE(vs);
	VALUE(qt_install);
	VALUE(qt_bin);

	VALUE(pf_x86);
	VALUE(pf_x64);
	VALUE(temp_dir);

#undef VALUE
};


class conf
{
public:
	conf_global global();
	conf_task task(const std::vector<std::string>& names);
	conf_tools tool();
	conf_transifex transifex();
	conf_prebuilt prebuilt();
	conf_versions version();
	conf_paths path();

	// this just forwards to conf_global, but it's used everywhere
	static bool dry();
};

}	// namespace

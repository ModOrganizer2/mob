#pragma once

// these shouldn't be called directly, they're used by some of the template
// below
//
namespace mob::details
{

// returns an option named `key` from the given `section`
//
std::string get_string(std::string_view section, std::string_view key);

// calls get_string(), converts to bool
//
bool get_bool(std::string_view section, std::string_view key);

// calls get_string(), converts to in
//
int get_int(std::string_view section, std::string_view key);

// sets the given option, bails out if the option doesn't exist
//
void set_string(
	std::string_view section, std::string_view key, std::string_view value);

}	// namespace


namespace mob
{

// reads options from the given inis and option strings, resolves all the paths
// and necessary tools, also adds a couple of things to PATH
//
void init_options(
	const std::vector<fs::path>& inis, const std::vector<std::string>& opts);

// checks some of the options once everything is loaded, returns false if
// something's wrong
//
bool verify_options();

// returns all options formatted as three columns
//
std::vector<std::string> format_options();


// base class for all conf structs
//
template <class DefaultType>
class conf_section
{
public:
	DefaultType get(std::string_view key) const
	{
		return details::get_string(name_, key);
	}

	// undefined
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


// options in [global]
//
class conf_global : public conf_section<std::string>
{
public:
	conf_global();

	// convenience, doesn't need string manipulation
	int output_log_level() const;
	int file_log_level() const;
	bool dry() const;

	// convenience
	bool redownload()   const { return get<bool>("redownload"); }
	bool reextract()    const { return get<bool>("reextract"); }
	bool reconfigure()  const { return get<bool>("reconfigure"); }
	bool rebuild()      const { return get<bool>("rebuild"); }
	bool clean()        const { return get<bool>("clean_task"); }
	bool fetch()        const { return get<bool>("fetch_task"); }
	bool build()        const { return get<bool>("build_task"); }
};


// options in [task] or [task_name:task]
//
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


// options in [tools]
//
class conf_tools : public conf_section<fs::path>
{
public:
	conf_tools();
};


// options in [transifex]
//
class conf_transifex : public conf_section<std::string>
{
public:
	conf_transifex();
};


// options in [versions]
//
class conf_versions : public conf_section<std::string>
{
public:
	conf_versions();
};


// options in [prebuilt]
//
class conf_prebuilt : public conf_section<std::string>
{
public:
	conf_prebuilt();
};


// options in [paths]
//
class conf_paths : public conf_section<fs::path>
{
public:
	conf_paths();

#define VALUE(NAME) fs::path NAME() const { return get(#NAME); }

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


// should be used as conf().global().whatever(), doesn't actually hold anything,
// but it's better than a bunch of static functions
//
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
};

}	// namespace

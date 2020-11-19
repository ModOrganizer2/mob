#pragma once

#include "../utility.h"

namespace mob
{

// a set of environment variables; copy-on-write because this gets copied a lot
//
class env
{
public:
	using map = std::map<std::wstring, std::wstring>;

	// used in set(); replaces, appends or prepends to a variable if it already
	// exists
	//
	enum flags
	{
		replace = 1,
		append,
		prepend
	};

	// Visual Studio environment variables for 32-bit
	//
	static env vs_x86();

	// Visual Studio environment variables for 64-bit
	//
	static env vs_x64();

	// Visual Studio environment variables for the given architecture
	//
	static env vs(arch a);


	// empty set
	//
	env();

	// handle ref count
	//
	env(const env& e);
	env(env&& e);
	env& operator=(const env& e);
	env& operator=(env&& e);

	// prepends to PATH
	//
	env& prepend_path(const fs::path& p);
	env& prepend_path(const std::vector<fs::path>& v);

	// appends to PATH
	//
	env& append_path(const fs::path& p);
	env& append_path(const std::vector<fs::path>& v);

	// sets k=v
	//
	env& set(std::string_view k, std::string_view v, flags f=replace);
	env& set(std::wstring k, std::wstring v, flags f=replace);

	// returns the variable' value, empty if not found
	//
	std::string get(std::string_view k) const;

	// map of variables
	//
	map get_map() const;

	// passed to CreateProcess() in the process class; returns a pointer to a
	// block of utf16 strings, owned by this, created on demand
	//
	void* get_unicode_pointers() const;

private:
	// shared between copies
	//
	struct data
	{
		std::mutex m;
		map vars;

		// unicode strings, see get_unicode_pointers()
		mutable std::wstring sys;
	};

	// shared data
	std::shared_ptr<data> data_;

	// whether this instance owns the data, set to true in copy_for_write()
	// when the data must be modified
	bool own_;


	// creates the unicode strings
	//
	void create_sys() const;

	// returns a pointer inside the map, null if not found
	//
	std::wstring* find(std::wstring_view name);
	const std::wstring* find(std::wstring_view name) const;

	// called by set(), sets the value in the map
	//
	void set_impl(std::wstring k, std::wstring v, flags f);

	// duplicates the data, sets own_=true
	//
	void copy_for_write();

	// called by the various *_path() functions, actually changes the PATH
	// value
	//
	env& change_path(const std::vector<fs::path>& v, flags f);
};


// represents mob's environment variables
//
struct this_env
{
	// sets a variable
	//
	static void set(
		const std::string& k,
		const std::string& v,
		env::flags f=env::replace);

	// changes PATH
	//
	static void prepend_to_path(const fs::path& p);
	static void append_to_path(const fs::path& p);

	// returns mob's environment variables
	//
	static env get();

	// returns a specific variable; bails out if it doesn't exist
	//
	static std::string get(const std::string& k);

	// returns a specific variable, or empty if it doesn't exist
	//
	static std::optional<std::string> get_opt(const std::string& k);

private:
	// used by get() and get_opt(), does the actual work
	//
	static std::optional<std::wstring> get_impl(const std::string& k);
};

}	// namespace

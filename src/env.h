#pragma once

#include "utility.h"

namespace mob
{

class env
{
public:
	using map = std::map<std::wstring, std::wstring>;

	enum flags
	{
		replace = 1,
		append,
		prepend
	};

	static env vs_x86();
	static env vs_x64();
	static env vs(arch a);

	env();
	env(const env& e);
	env(env&& e);
	env& operator=(const env& e);
	env& operator=(env&& e);

	env& prepend_path(const fs::path& p);
	env& prepend_path(const std::vector<fs::path>& v);
	env& append_path(const fs::path& p);
	env& append_path(const std::vector<fs::path>& v);

	env& set(std::string_view k, std::string_view v, flags f=replace);
	env& set(std::wstring k, std::wstring v, flags f=replace);

	void set_from(const env& e);

	std::string get(std::string_view k) const;
	map get_map() const;

	void* get_unicode_pointers() const;

private:
	struct data
	{
		std::mutex m;
		map vars;
		mutable std::wstring sys;
	};

	std::shared_ptr<data> data_;
	bool own_;

	void create() const;
	std::wstring* find(std::wstring_view name);
	const std::wstring* find(std::wstring_view name) const;
	void set_impl(std::wstring k, std::wstring v, flags f);
	void copy_for_write();
	env& change_path(const std::vector<fs::path>& v, flags f);
};


struct this_env
{
	static void set(
		const std::string& k,
		const std::string& v,
		env::flags f=env::replace);

	static void prepend_to_path(const fs::path& p);
	static void append_to_path(const fs::path& p);

	static env get();

	static std::string get(const std::string& k);
	static std::optional<std::string> get_opt(const std::string& k);

private:
	static std::optional<std::wstring> get_impl(const std::string& k);
};

}	// namespace

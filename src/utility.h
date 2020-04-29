#pragma once

namespace builder
{

class url;

class bailed
{
public:
	bailed(std::string s)
		: s_(std::move(s))
	{
	}

	const char* what() const
	{
		return s_.c_str();
	}

private:
	std::string s_;
};


struct handle_closer
{
	using pointer = HANDLE;

	void operator()(HANDLE h)
	{
		if (h != INVALID_HANDLE_VALUE)
			::CloseHandle(h);
	}
};

using handle_ptr = std::unique_ptr<HANDLE, handle_closer>;


struct file_closer
{
	void operator()(std::FILE* f)
	{
		if (f)
			std::fclose(f);
	}
};

using file_ptr = std::unique_ptr<FILE, file_closer>;


class file_deleter
{
public:
	file_deleter(fs::path p);
	file_deleter(const file_deleter&) = delete;
	file_deleter& operator=(const file_deleter&) = delete;
	~file_deleter();

	void delete_now();
	void cancel();

private:
	fs::path p_;
	bool delete_;
};


class directory_deleter
{
public:
	directory_deleter(fs::path p);
	directory_deleter(const directory_deleter&) = delete;
	directory_deleter& operator=(const directory_deleter&) = delete;
	~directory_deleter();

	void delete_now();
	void cancel();

private:
	fs::path p_;
	bool delete_;
};


enum class level
{
	debug, info, warning, error, bail
};


std::string error_message(DWORD e);

void out(level lv, const std::string& s);
void out(level lv, const std::string& s, DWORD e);
void out(level lv, const std::string& s, const std::error_code& e);
void dump_logs();

template <class... Args>
[[noreturn]] void bail_out(Args&&... args)
{
	out(level::bail, std::forward<Args>(args)...);
}

template <class... Args>
void error(Args&&... args)
{
	out(level::error, std::forward<Args>(args)...);
}

template <class... Args>
void warn(Args&&... args)
{
	out(level::warning, std::forward<Args>(args)...);
}

template <class... Args>
void info(Args&&... args)
{
	out(level::info, std::forward<Args>(args)...);
}

template <class... Args>
void debug(Args&&... args)
{
	out(level::debug, std::forward<Args>(args)...);
}


std::string env(const std::string& name);
void env(const std::string& name, const std::string& value);
void prepend_to_path(const fs::path& dir);

std::string redir_nul();

std::string read_text_file(const fs::path& p);

std::string replace_all(
	std::string s, const std::string& from, const std::string& to);

std::string join(const std::vector<std::string>& v, const std::string& sep);


class cmd
{
public:
	enum arg_flags
	{
		noargflags = 0x00,
		quiet   = 0x01,
		nospace = 0x02
	};

	enum flags
	{
		noflags = 0x00,
		stdout_is_verbose = 0x01
	};


	cmd(const fs::path& exe, flags f)
		: exe_(exe.filename().string()), flags_(f)
	{
		s_ += arg_to_string(exe);
	}

	cmd& name(const std::string& s);
	const std::string& name() const;

	cmd& cwd(const fs::path& p);
	const fs::path& cwd() const;

	template <class T>
	cmd& arg(const T& value, arg_flags f=noargflags)
	{
		add_arg("", arg_to_string(value), f);
		return *this;
	}

	template <class T>
	cmd& arg(const std::string& name, const T& value, arg_flags f=noargflags)
	{
		add_arg(name, arg_to_string(value), f);
		return *this;
	}

	std::string string() const;

private:
	std::string name_;
	std::string exe_;
	fs::path cwd_;
	std::string s_;
	flags flags_;

	void add_arg(const std::string& k, const std::string& v, arg_flags f);

	std::string arg_to_string(const char* s);
	std::string arg_to_string(const std::string& s);
	std::string arg_to_string(const fs::path& p);
	std::string arg_to_string(const url& u);
};

}	// namespace

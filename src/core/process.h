#pragma once

#include "env.h"
#include "context.h"
#include "../utility.h"

namespace mob
{

class url;
class async_pipe_stdout;
class async_pipe_stdin;

class process
{
public:
	static constexpr DWORD wait_timeout = 50;

	enum flags_t
	{
		noflags                  = 0x00,
		allow_failure            = 0x01,
		terminate_on_interrupt   = 0x02,
		ignore_output_on_success = 0x04
	};

	enum arg_flags
	{
		noargflags      = 0x00,
		log_debug       = 0x01,
		log_trace       = 0x02,
		log_dump        = 0x04,
		log_quiet       = 0x08,
		nospace         = 0x10,
		quote           = 0x20,
		forward_slashes = 0x40
	};

	enum stream_flags
	{
		forward_to_log = 1,
		bit_bucket,
		keep_in_string,
		inherit
	};

	struct filter
	{
		std::string_view line;
		context::reason r;
		context::level lv;
		bool ignore;

		filter(
			std::string_view line, context::reason r, context::level lv,
			bool ignore)
				: line(line), r(r), lv(lv), ignore(ignore)
		{
		}


		// this is only used by filter_fun, where copying a `filter` wouldn't
		// make any sense
		filter(const filter&) = delete;
		filter& operator=(const filter&) = delete;
	};

	using filter_fun = std::function<void (filter&)>;


	process();
	~process();

	// anchors
	process(process&&);
	process(const process&);
	process& operator=(const process&);
	process& operator=(process&&);

	static process raw(const context& cx, const std::string& cmd);

	static process pipe(process p)
	{
		return p;
	}

	template <class... Processes>
	static process pipe(const process& p1, const process& p2, Processes&&... ps)
	{
		auto r = p1;
		r.pipe_into(p2);
		pipe(r, std::forward<Processes>(ps)...);
		return r;
	}

	process& set_context(const context* cx);

	process& name(const std::string& name);
	std::string name() const;

	process& binary(const fs::path& p);
	const fs::path& binary() const;

	process& cwd(const fs::path& p);
	const fs::path& cwd() const;

	process& stdout_flags(stream_flags s);
	process& stdout_level(context::level lv);
	process& stdout_filter(filter_fun f);
	process& stdout_encoding(encodings e);

	process& stderr_flags(stream_flags s);
	process& stderr_level(context::level lv);
	process& stderr_filter(filter_fun f);
	process& stderr_encoding(encodings e);

	process& stdin_string(std::string s);

	process& chcp(int cp);
	process& cmd_unicode(bool b);

	process& external_error_log(const fs::path& p);

	process& flags(flags_t f);
	flags_t flags() const;

	process& success_exit_codes(std::set<int> v);

	template <class T, class=std::enable_if_t<!std::is_same_v<T, arg_flags>>>
	process& arg(const T& value, arg_flags f=noargflags)
	{
		add_arg("", arg_to_string(value, f), f);
		return *this;
	}

	template <class T, class=std::enable_if_t<!std::is_same_v<T, arg_flags>>>
	process& arg(const std::string& name, const T& value, arg_flags f=noargflags)
	{
		add_arg(name, arg_to_string(value, f), f);
		return *this;
	}

	template <template<class, class> class Container, class K, class V, class Alloc>
	process& args(const Container<std::pair<K, V>, Alloc>& v, arg_flags f=noargflags)
	{
		for (auto&& [name, value] : v)
			arg(name, value, f);

		return *this;
	}

	template <class Container>
	process& args(const Container& v, arg_flags f=noargflags)
	{
		for (auto&& e : v)
			add_arg(e, "", f);

		return *this;
	}

	process& env(const mob::env& e);

	void run();
	void interrupt();
	void join();

	int exit_code() const;
	std::string stdout_string();
	std::string stderr_string();

private:
	struct impl
	{
		handle_ptr handle;
		handle_ptr job;
		std::atomic<bool> interrupt{false};
		std::unique_ptr<async_pipe_stdout> stdout_pipe;
		std::unique_ptr<async_pipe_stdout> stderr_pipe;
		std::unique_ptr<async_pipe_stdin> stdin_pipe;
		handle_ptr stdin_handle;

		impl() = default;
		impl(const impl&);
		impl& operator=(const impl&);
	};

	struct stream
	{
		stream_flags flags = forward_to_log;
		context::level level = context::level::trace;
		filter_fun filter;
		encodings encoding = encodings::dont_know;
		encoded_buffer buffer;

		stream(context::level lv)
			: level(lv)
		{
		}
	};

	struct io
	{
		bool unicode;
		int chcp;
		stream out;
		stream err;
		std::optional<std::string> in;
		std::size_t in_offset;
		fs::path error_log_file;
		std::map<context::level, std::vector<std::string>> logs;

		io();
	};

	struct exec
	{
		fs::path bin;
		fs::path cwd;
		mob::env env;
		std::set<int> success;
		std::string raw;
		std::string cmd;
		DWORD code;

		exec();
	};

	const context* cx_;
	std::string name_;
	flags_t flags_;
	impl impl_;
	io io_;
	exec exec_;

	std::string make_name() const;
	std::string make_cmd() const;
	std::wstring make_cmd_args(const std::string& what) const;
	void pipe_into(const process& p);

	void do_run(const std::string& what);
	void read_pipes(bool finish);
	void read_pipe(bool finish, stream& s, async_pipe_stdout& pipe, context::reason r);

	void on_completed();
	void on_timeout(bool& already_interrupted);
	void terminate();

	void dump_error_log_file() noexcept;
	void dump_stderr() noexcept;

	void add_arg(const std::string& k, const std::string& v, arg_flags f);

	std::string arg_to_string(const char* s, arg_flags f);
	std::string arg_to_string(const std::string& s, arg_flags f);
	std::string arg_to_string(const fs::path& p, arg_flags f);
	std::string arg_to_string(const url& u, arg_flags f);
	std::string arg_to_string(int i, arg_flags f);
};


MOB_ENUM_OPERATORS(process::flags_t);

}	// namespace

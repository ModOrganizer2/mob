#pragma once

#include "utility.h"
#include "env.h"
#include "context.h"

namespace mob
{

class url;


class async_pipe
{
public:
    async_pipe();

    handle_ptr create();
    std::string_view read();

private:
    static const std::size_t buffer_size = 50'000;

    handle_ptr stdout_;
    handle_ptr event_;
    std::unique_ptr<char[]> buffer_;
    OVERLAPPED ov_;
    bool pending_;

    HANDLE create_pipe();
    std::string_view try_read();
    std::string_view check_pending();
};


class process
{
public:
	enum flags_t
	{
		noflags                = 0x00,
		allow_failure          = 0x01,
		terminate_on_interrupt = 0x02
	};

	enum arg_flags
	{
		noargflags = 0x00,
		log_debug  = 0x01,
		log_trace  = 0x02,
		log_dump   = 0x04,
		log_quiet  = 0x08,
		nospace    = 0x10,
		quote      = 0x20
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
		const std::string_view line;
		context::reason r;
		context::level lv;
		bool ignore;
	};

	using filter_fun = std::function<void (filter&)>;


	process();
	~process();

	static process raw(const context& cx, const std::string& cmd);

	static process pipe(const process& p)
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

	process& stderr_flags(stream_flags s);
	process& stderr_level(context::level lv);
	process& stderr_filter(filter_fun f);

	process& external_error_log(const fs::path& p);

	process& flags(flags_t f);
	flags_t flags() const;

	template <class T, class=std::enable_if_t<!std::is_same_v<T, arg_flags>>>
	process& arg(const T& value, arg_flags f=noargflags)
	{
		add_arg("", arg_to_string(value, (f & quote)), f);
		return *this;
	}

	template <class T, class=std::enable_if_t<!std::is_same_v<T, arg_flags>>>
	process& arg(const std::string& name, const T& value, arg_flags f=noargflags)
	{
		add_arg(name, arg_to_string(value, (f & quote)), f);
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
		std::atomic<bool> interrupt{false};
		async_pipe stdout_pipe, stderr_pipe;

		impl() = default;
		impl(const impl&);
		impl& operator=(const impl&);
	};

	struct stream
	{
		stream_flags flags = forward_to_log;
		context::level level = context::level::trace;
		filter_fun filter;
		std::string string;
	};

	const context* cx_;
	std::string name_;
	fs::path bin_;
	fs::path cwd_;
	flags_t flags_;
	stream stdout_;
	stream stderr_;
	mob::env env_;
	std::string raw_;
	std::string cmd_;
	fs::path error_log_file_;

	impl impl_;
	DWORD code_;

	std::string make_name() const;
	std::string make_cmd() const;
	void pipe_into(const process& p);

	void do_run(const std::string& what);
	bool read_pipes();
	bool read_pipe(stream& s, async_pipe& pipe, context::reason r);

	void on_completed();
	void on_timeout(bool& already_interrupted);
	void dump_error_log_file() noexcept;

	void add_arg(const std::string& k, const std::string& v, arg_flags f);

	std::string arg_to_string(const char* s, bool force_quote);
	std::string arg_to_string(const std::string& s, bool force_quote);
	std::string arg_to_string(const fs::path& p, bool force_quote);
	std::string arg_to_string(const url& u, bool force_quote);
};


MOB_ENUM_OPERATORS(process::flags_t);

}	// namespace

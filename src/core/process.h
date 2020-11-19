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

	// given in flags(), control process creation and termination
	//
	enum process_flags
	{
		noflags                  = 0x00,

		// will not bail out on failure, used for optional processes
		allow_failure            = 0x01,

		// some processes just refuse to die when given sigint, like jom, so
		// this just kills the process
		terminate_on_interrupt   = 0x02,

		// some processes output useless stuff even when they're successful,
		// so to try to keep the amount of logs down, specifying this will
		// discard the output of the process is successful
		ignore_output_on_success = 0x04
	};


	// used in arg(), controls how args are converted to string and whether
	// they're tied to a specific log level
	//
	enum arg_flags
	{
		noargflags      = 0x00,

		// the argument should only be used when the given log level is active;
		// a /q switch for quiet output would have the log_quiet flag, for
		// example
		log_debug       = 0x01,
		log_trace       = 0x02,
		log_dump        = 0x04,
		log_quiet       = 0x08,

		// for arg(k, v), doesn't put a space between `k` and `v`; some programs
		// are pretty strict with their arguments, like 7z working for `-opath`
		// but not `-o path`
		nospace         = 0x10,

		// for arg(k, v) or arg(v), forces the value to be double-quoted (but
		// not the key); note that fs::path and url objects are always quoted
		// automatically
		quote           = 0x20,

		// converts backslashes to forward slashes for the given fs::path
		// value; ignored for other types
		forward_slashes = 0x40
	};


	// used in stdout_flags() and stderr_flags(), controls what to do with the
	// process' output
	//
	enum stream_flags
	{
		// default, forwards the output directly to mob's logs, both file and
		// console, depending on the configuration
		forward_to_log = 1,

		// discards the output
		bit_bucket,

		// does not log the output, keeps it in a string; can be retrieved in
		// stdout_string() and stderr_string()
		keep_in_string,

		// inherits stdout/stderr from this process; this is only used by
		// processes started early in mob before things are set up, like when
		// calling vswhere, so it just dumps stuff to the console
		inherit
	};


	// used to filter the output of a process, because some programs really suck
	// at being quiet and will output crap that's not needed but cannot be
	// inhibited
	//
	// a lambda is given to stdout_filter() and stderr_filter() that will
	// receive a `filter` object, allowing to change the log level of a line
	// or just discard it completely
	//
	struct filter
	{
		// a line from the process' output
		std::string_view line;

		// the base reason for the stream, either context::std_out or
		// context::std_err, can be modified by the filter
		context::reason r;

		// the base level for the stream, defaults to stdout=trace and
		// stderr=error, or whatever was given to stdout_level()/stderr_level()
		// below; can be modified by the filter
		context::level lv;

		// whether to discard this log line; can be set to true by the filter
		bool discard;

		filter(std::string_view line, context::reason r, context::level lv);

		// this struct is only used by filter_fun callback, where copying a
		// filter wouldn't make any sense
		filter(const filter&) = delete;
		filter& operator=(const filter&) = delete;
	};

	using filter_fun = std::function<void (filter&)>;


	// empty process
	//
	process();

	// joins
	//
	~process();

	// anchors, all defaults
	process(process&&);
	process(const process&);
	process& operator=(const process&);
	process& operator=(process&&);


	// creates a process from the given command line instead of using the
	// various binary(), arg(), etc.
	//
	static process raw(const context& cx, const std::string& cmd);

	// used by pipe(...) below to finish recursion
	//
	static process pipe(process p)
	{
		return p;
	}

	// constructs a process object by concatenating the command line of the
	// given processes with " | " in between; this can only be used with fully
	// set up processes, their command line is extracted immediately
	//
	// it's basically only used by 7z to pipe tar into it and is not a very
	// generic solution
	//
	template <class... Processes>
	static process pipe(const process& p1, const process& p2, Processes&&... ps)
	{
		auto r = p1;
		r.pipe_into(p2);
		pipe(r, std::forward<Processes>(ps)...);
		return r;
	}

	// sets the context of this process, used for all logging, bailing out,
	// filesystem operations, etc.; the process runners use this before spawning
	// the process
	//
	process& set_context(const context* cx);

	// display name for the process, used in logging; if not set, returns the
	// filename without extension of the binary, which may be an empty string
	//
	process& name(const std::string& name);
	std::string name() const;

	// path to the executable
	//
	process& binary(const fs::path& p);
	const fs::path& binary() const;

	// working directory
	//
	process& cwd(const fs::path& p);
	const fs::path& cwd() const;

	// process flags
	//
	process& flags(process_flags f);
	process_flags flags() const;

	// sets flags for stdout/stderr
	//
	process& stdout_flags(stream_flags s);
	process& stderr_flags(stream_flags s);

	// sets the default log level for stdout/stderr; if not given, stdout is
	// trace, stderr is error
	//
	process& stdout_level(context::level lv);
	process& stderr_level(context::level lv);

	// sets a callback to filter the output
	//
	process& stdout_filter(filter_fun f);
	process& stderr_filter(filter_fun f);

	// sets the encoding of stdout/stderr, defaults to dont_know, which doesn't
	// do any conversion
	//
	process& stdout_encoding(encodings e);
	process& stderr_encoding(encodings e);

	// if the string is not empty, the process' stdin will be redirected and
	// given the string
	//
	process& stdin_string(std::string s);

	// if not -1, `chcp cp` will be executed before spawning the process; note
	// that processes are started in their own cmd instance, so this won't leak
	//
	process& chcp(int cp);

	// passes /U to cmd when spawning the process
	//
	// this is basically only used when getting the environment variables after
	// calling vcvars to force `set` to output in utf16, because it normally
	// outputs in the current codepage
	//
	// also forces stdout_encoding() and stderr_encoding() to utf16
	//
	process& cmd_unicode(bool b);

	// some processes output to an external log file instead of to
	// stdout/stderr, such as boost's boostrap.bat
	//
	// if the process failed (returned an exit code considered failure), the
	// content of the file will be dumped to the logs as errors; if the process
	// succeeded, the file is ignored
	//
	// the file is always deleted before starting the process
	//
	process& external_error_log(const fs::path& p);

	// the default success exit code is 0, this can be used to override it; any
	// exit code found in the set is considered success
	//
	// basically only used by transifex `init`, which exits with 2 when the
	// directory already has a .tx folder
	//
	process& success_exit_codes(const std::set<int>& v);


	// adds an argument to the command line, see comment on top for conversions
	//
	template <class T, class=std::enable_if_t<!std::is_same_v<T, arg_flags>>>
	process& arg(const T& value, arg_flags f=noargflags)
	{
		add_arg("", arg_to_string(value, f), f);
		return *this;
	}

	// adds a name=value argument to the command line, see comment on top for
	// conversions
	//
	template <class T, class=std::enable_if_t<!std::is_same_v<T, arg_flags>>>
	process& arg(const std::string& name, const T& value, arg_flags f=noargflags)
	{
		add_arg(name, arg_to_string(value, f), f);
		return *this;
	}

	// adds every name=value pair to the command line, see comment on top for
	// conversions
	//
	template <template<class, class> class Container, class K, class V, class Alloc>
	process& args(const Container<std::pair<K, V>, Alloc>& v, arg_flags f=noargflags)
	{
		for (auto&& [name, value] : v)
			arg(name, value, f);

		return *this;
	}

	// adds every string from the container to the command line as-is
	//
	process& args(const std::vector<std::string>& v, arg_flags f=noargflags)
	{
		for (auto&& e : v)
			add_arg(e, "", f);

		return *this;
	}

	// sets the environment variables for this process
	//
	// this can use something like this_env() to add variables, or env::vs()
	// to get a VS environment
	//
	process& env(const mob::env& e);

	// spawns the process, returns immediately; bails out if it fails
	//
	// join() must be called shortly after, that's where the process is
	// monitored and redirected streams handled
	//
	void run();

	// forces the process to exit by sending sigint or killing it, depending
	// on process flags; this just sets a flag, join() does the work
	//
	void interrupt();

	// reads from streams, writes to stdin if needed, monitors for termination,
	// handles interrupt(); bails out on failure
	//
	void join();


	// exit code of the process, only valid after join() returns
	//
	int exit_code() const;

	// content of stdout/stderr if keep_in_string is set
	//
	std::string stdout_string();
	std::string stderr_string();

private:
	// stuff that must be handled when copying process objects
	//
	struct impl
	{
		// process handle
		handle_ptr handle;

		// job handle; processes are added to a job so child processes can be
		// monitored and terminated
		handle_ptr job;

		// whether the process should be killed
		std::atomic<bool> interrupt{false};

		// pipes
		std::unique_ptr<async_pipe_stdout> stdout_pipe;
		std::unique_ptr<async_pipe_stdout> stderr_pipe;
		std::unique_ptr<async_pipe_stdin> stdin_pipe;

		impl() = default;
		impl(const impl&);
		impl& operator=(const impl&);
	};

	// info about stdout/stderr
	//
	struct stream
	{
		stream_flags flags;
		context::level level;
		filter_fun filter;
		encodings encoding;

		// anything output to stdout/stderr ends up here
		encoded_buffer buffer;

		stream(context::level lv)
			: flags(forward_to_log), level(lv), encoding(encodings::dont_know)
		{
		}
	};

	// info about the process io
	//
	struct io
	{
		// whether /U is given to cmd, see cmd_unicode()
		bool unicode;

		// if not -1, calls chcp before spawning, see chcp()
		int chcp;

		// stdout/stderr
		stream out;
		stream err;

		// stdin string that's fed to the process
		std::optional<std::string> in;

		// index of last character written to stdin
		std::size_t in_offset;

		// see external_error_log()
		fs::path error_log_file;

		// each line from the process is saved in this map so it can be output
		// after the process has completed successfully but had stuff in stderr
		std::map<context::level, std::vector<std::string>> logs;

		io();
	};

	// info about execution
	//
	struct exec
	{
		fs::path bin;
		fs::path cwd;
		mob::env env;

		// success exit codes, defaults to 0
		std::set<int> success;

		// set in process::raw() or process::pipe(), used instead of cmd
		std::string raw;

		// built by calling arg() or args()
		std::string cmd;

		// exit code
		DWORD code;

		exec();
	};

	// log context
	const context* cx_;

	// display name
	std::string name_;

	// flags
	process_flags flags_;

	// non copyable stuff
	impl impl_;

	// to avoid having 30 member variables
	io io_;
	exec exec_;


	// returns name() or the command line
	//
	std::string make_name() const;

	// the command line for the process itself
	//
	std::string make_cmd() const;

	// returns arguments given to cmd, `what` is the whole command line for
	// the process itself; this includes flags to cmd like /U, but also stuff
	// like chcp
	//
	std::wstring make_cmd_args(const std::string& what) const;

	// sets the raw command line to `make_cmd() | p.make_cmd()`
	//
	void pipe_into(const process& p);


	// builds the command line, sets up redirections and and calls
	// CreateProcess()
	//
	void do_run(const std::string& what);

	// deletes the external log file, if any
	//
	void delete_external_log_file();

	// creates the job object
	//
	void create_job();

	// sets up stdout/stderr/stdin redirection
	//
	handle_ptr redirect_stdout(STARTUPINFOW& si);
	handle_ptr redirect_stderr(STARTUPINFOW& si);
	handle_ptr redirect_stdin(STARTUPINFOW& si);

	// calls CreateProcess() with the given stuff
	//
	void create(
		std::wstring cmd, std::wstring args, std::wstring cwd, STARTUPINFOW si);


	// called regularly in join(), checks for termination or interruption,
	// handles pipes
	//
	void on_timeout(bool& already_interrupted);

	// reads from stdin and stderr, `finish` must be true when the process has
	// terminated
	//
	void read_pipes(bool finish);

	// reads from the given stream and pipe, `finish` must be true when the
	// process has terminated
	//
	void read_pipe(
		bool finish, stream& s,
		async_pipe_stdout& pipe, context::reason r);

	// sends stuff to stdin, if any
	//
	void feed_stdin();


	// called when the process has terminated; checks exit code, logs stuff and
	// bails out on errors
	//
	void on_completed();

	// called from on_completed() when the process' exit code was successful
	//
	void on_process_successful();

	// called from on_completed() when the process' exit code was a failure
	//
	void on_process_failed();

	// interrupts the process if needed, returns true if interrupted
	//
	bool check_interrupted();

	// forcefully kills the process and its children
	//
	void terminate();


	// if external_error_log() was called, dumps the contenf of the log file as
	// errors
	//
	void dump_error_log_file() noexcept;

	// logs the content of stderr, used when the process failed before bailing
	// out
	//
	void dump_stderr() noexcept;


	// adds the given argument to the command line
	//
	// depending on the flags, the argument may be discarded; for example, if
	// the argument is marked as arg_flags::log_quiet but the log level is
	// trace, it is not included
	//
	void add_arg(const std::string& k, const std::string& v, arg_flags f);

	// argument conversions
	//
	static std::string arg_to_string(const char* s, arg_flags f);
	static std::string arg_to_string(const std::string& s, arg_flags f);
	static std::string arg_to_string(const fs::path& p, arg_flags f);
	static std::string arg_to_string(const url& u, arg_flags f);
	static std::string arg_to_string(int i, arg_flags f);
};


MOB_ENUM_OPERATORS(process::process_flags);

}	// namespace

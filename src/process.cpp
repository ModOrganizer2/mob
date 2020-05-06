#include "pch.h"
#include "process.h"
#include "conf.h"
#include "net.h"
#include "context.h"
#include "op.h"

namespace mob
{

HANDLE get_bit_bucket()
{
	SECURITY_ATTRIBUTES sa { .nLength = sizeof(sa), .bInheritHandle = TRUE };
	return ::CreateFileA("NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, 0);
}


async_pipe::async_pipe()
	: pending_(false)
{
	buffer_ = std::make_unique<char[]>(buffer_size);
	std::memset(buffer_.get(), 0, buffer_size);

	std::memset(&ov_, 0, sizeof(ov_));
}

handle_ptr async_pipe::create()
{
	// creating pipe
	handle_ptr out(create_pipe());
	if (out.get() == INVALID_HANDLE_VALUE)
		return {};

	ov_.hEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (ov_.hEvent == NULL)
	{
		const auto e = GetLastError();
		bail_out("CreateEvent failed", e);
	}

	event_.reset(ov_.hEvent);

	return out;
}

std::string_view async_pipe::read()
{
	if (pending_)
		return check_pending();
	else
		return try_read();
}

HANDLE async_pipe::create_pipe()
{
	static std::atomic<int> pipe_id(0);

	const std::string pipe_name_prefix = "\\\\.\\pipe\\mob_pipe";
	const std::string pipe_name = pipe_name_prefix + std::to_string(++pipe_id);

	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;

	handle_ptr pipe;

	// creating pipe
	{
		HANDLE pipe_handle = ::CreateNamedPipeA(
			pipe_name.c_str(), PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,
			1, 50'000, 50'000, pipe_timeout, &sa);

		if (pipe_handle == INVALID_HANDLE_VALUE)
		{
			const auto e = GetLastError();
			bail_out("CreateNamedPipe failed", e);
		}

		pipe.reset(pipe_handle);
	}

	{
		// duplicating the handle to read from it
		HANDLE output_read = INVALID_HANDLE_VALUE;

		const auto r = DuplicateHandle(
			GetCurrentProcess(), pipe.get(), GetCurrentProcess(), &output_read,
			0, TRUE, DUPLICATE_SAME_ACCESS);

		if (!r)
		{
			const auto e = GetLastError();
			bail_out("DuplicateHandle for pipe", e);
		}

		stdout_.reset(output_read);
	}


	// creating handle to pipe which is passed to CreateProcess()
	HANDLE output_write = ::CreateFileA(
		pipe_name.c_str(), FILE_WRITE_DATA|SYNCHRONIZE, 0,
		&sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (output_write == INVALID_HANDLE_VALUE)
	{
		const auto e = GetLastError();
		bail_out("CreateFileW for pipe failed", e);
	}

	return output_write;
}

std::string_view async_pipe::try_read()
{
	DWORD bytes_read = 0;

	if (!::ReadFile(stdout_.get(), buffer_.get(), buffer_size, &bytes_read, &ov_))
	{
		const auto e = GetLastError();

		switch (e)
		{
			case ERROR_IO_PENDING:
			{
				pending_ = true;
				break;
			}

			case ERROR_BROKEN_PIPE:
			{
				// broken pipe probably means lootcli is finished
				break;
			}

			default:
			{
				bail_out("async_pipe read failed", e);
				break;
			}
		}

		return {};
	}

	return {buffer_.get(), bytes_read};
}

std::string_view async_pipe::check_pending()
{
	DWORD bytes_read = 0;

	const auto r = WaitForSingleObject(event_.get(), pipe_timeout);

	if (r == WAIT_FAILED) {
		const auto e = GetLastError();
		bail_out("WaitForSingleObject in async_pipe failed", e);
	}

	if (!::GetOverlappedResult(stdout_.get(), &ov_, &bytes_read, FALSE))
	{
		const auto e = GetLastError();

		switch (e)
		{
			case ERROR_IO_INCOMPLETE:
			{
				break;
			}

			case WAIT_TIMEOUT:
			{
				break;
			}

			case ERROR_BROKEN_PIPE:
			{
				// broken pipe probably means lootcli is finished
				break;
			}

			default:
			{
				bail_out("GetOverlappedResult failed in async_pipe", e);
				break;
			}
		}

		return {};
	}

	::ResetEvent(event_.get());
	pending_ = false;

	return {buffer_.get(), bytes_read};
}


process::impl::impl(const impl& i)
	: interrupt(i.interrupt.load())
{
}

process::impl& process::impl::operator=(const impl& i)
{
	interrupt = i.interrupt.load();
	return *this;
}


process::process() :
	cx_(&gcx()), flags_(process::noflags),
	stdout_flags_(process::forward_to_log),
	stdout_level_(context::level::trace),
	stderr_flags_(process::forward_to_log),
	stderr_level_(context::level::error),
	code_(0)
{
}

process::~process()
{
	try
	{
		join();
	}
	catch(...)
	{
	}
}

process process::raw(const context& cx, const std::string& cmd)
{
	process p;
	p.cx_ = &cx;
	p.raw_ = cmd;
	return p;
}

process& process::set_context(const context* cx)
{
	cx_ = cx;
	return *this;
}

process& process::name(const std::string& name)
{
	name_ = name;
	return *this;
}

std::string process::name() const
{
	if (name_.empty())
		return bin_.stem().string();
	else
		return name_;
}

process& process::binary(const fs::path& p)
{
	bin_ = p;
	return *this;
}

const fs::path& process::binary() const
{
	return bin_;
}

process& process::cwd(const fs::path& p)
{
	cwd_ = p;
	return *this;
}

const fs::path& process::cwd() const
{
	return cwd_;
}

process& process::stdout_flags(stream_flags s)
{
	stdout_flags_ = s;
	return *this;
}

process& process::stdout_level(context::level lv)
{
	stdout_level_ = lv;
	return *this;
}

process& process::stdout_filter(filter_fun f)
{
	stdout_filter_ = f;
	return *this;
}

process& process::stderr_flags(stream_flags s)
{
	stderr_flags_ = s;
	return *this;
}

process& process::stderr_level(context::level lv)
{
	stderr_level_ = lv;
	return *this;
}

process& process::stderr_filter(filter_fun f)
{
	stderr_filter_ = f;
	return *this;
}

process& process::external_error_log(const fs::path& p)
{
	error_log_file_ = p;
	return *this;
}

process& process::flags(flags_t f)
{
	flags_ = f;
	return *this;
}

process::flags_t process::flags() const
{
	return flags_;
}

process& process::env(const mob::env& e)
{
	env_ = e;
	return *this;
}

std::string process::make_name() const
{
	if (!name_.empty())
		return name_;

	return make_cmd();
}

std::string process::make_cmd() const
{
	if (!raw_.empty())
		return raw_;

	return "\"" + bin_.string() + "\"" + cmd_;
}

void process::pipe_into(const process& p)
{
	raw_ = make_cmd() + " | " + p.make_cmd();
}

void process::run()
{
	if (!cwd_.empty())
		cx_->debug(context::cmd, "> cd " + cwd_.string());

	const auto what = make_cmd();
	cx_->debug(context::cmd, "> " + what);

	if (conf::dry())
		return;

	do_run(what);
}

void process::do_run(const std::string& what)
{
	if (raw_.empty() && bin_.empty())
		cx_->bail_out(context::cmd, "process: nothing to run");

	if (fs::exists(error_log_file_))
	{
		cx_->trace(context::cmd,
			"external error log file " +
			error_log_file_.string() + " exists, deleting");

		op::delete_file(*cx_, error_log_file_, op::optional);
	}

	STARTUPINFOA si = { .cb=sizeof(si) };
	PROCESS_INFORMATION pi = {};

	handle_ptr stdout_pipe, stderr_pipe;

	switch (stdout_flags_)
	{
		case forward_to_log:
		case keep_in_string:
		{
			stdout_pipe = impl_.stdout_pipe.create();
			si.hStdOutput = stdout_pipe.get();
			break;
		}

		case bit_bucket:
		{
			si.hStdOutput = get_bit_bucket();
			break;
		}

		case inherit:
		{
			si.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
			break;
		}
	}

	switch (stderr_flags_)
	{
		case forward_to_log:
		case keep_in_string:
		{
			stderr_pipe = impl_.stderr_pipe.create();
			si.hStdError = stderr_pipe.get();
			break;
		}

		case bit_bucket:
		{
			si.hStdError = get_bit_bucket();
			break;
		}

		case inherit:
		{
			si.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);
			break;
		}
	}

	si.hStdInput = get_bit_bucket();
	si.dwFlags = STARTF_USESTDHANDLES;

	const std::string cmd = this_env::get("COMSPEC");
	const std::string args = "/C \"" + what + "\"";

	const char* cwd_p = nullptr;
	std::string cwd_s;

	if (!cwd_.empty())
	{
		op::create_directories(*cx_, cwd_);
		cwd_s = cwd_.string();
		cwd_p = (cwd_s.empty() ? nullptr : cwd_s.c_str());
	}

	cx_->trace(context::cmd, "creating process");

	const auto r = ::CreateProcessA(
		cmd.c_str(), const_cast<char*>(args.c_str()),
		nullptr, nullptr, TRUE, CREATE_NEW_PROCESS_GROUP,
		env_.get_pointers(), cwd_p, &si, &pi);

	if (!r)
	{
		const auto e = GetLastError();
		cx_->bail_out(context::cmd, "failed to start '" + cmd + "'", e);
	}

	cx_->trace(context::cmd, "pid " + std::to_string(pi.dwProcessId));

	::CloseHandle(pi.hThread);
	impl_.handle.reset(pi.hProcess);
}

void process::interrupt()
{
	impl_.interrupt = true;
	cx_->trace(context::cmd, "will interrupt");
}

void process::join()
{
	if (!impl_.handle)
		return;

	bool interrupted = false;
	guard g([&] { impl_.handle = {}; });

	cx_->trace(context::cmd, "joining");

	for (;;)
	{
		const auto r = WaitForSingleObject(impl_.handle.get(), 100);

		if (r == WAIT_OBJECT_0)
		{
			on_completed();
			break;
		}
		else if (r == WAIT_TIMEOUT)
		{
			on_timeout(interrupted);
		}
		else
		{
			const auto e = GetLastError();
			cx_->bail_out(context::cmd, "failed to wait on process", e);
		}
	}

	if (interrupted)
		cx_->trace(context::cmd, "process interrupted and finished");
}

void process::read_pipes()
{
	// stdout
	switch (stdout_flags_)
	{
		case forward_to_log:
		{
			std::string_view s = impl_.stdout_pipe.read();

			for_each_line(s, [&](auto&& line)
			{
				filter f = {line, context::std_out, stdout_level_, false};

				if (stdout_filter_)
				{
					stdout_filter_(f);
					if (f.ignore)
						return;
				}

				cx_->log(f.r, f.lv, f.line);
			});

			break;
		}

		case keep_in_string:
		{
			std::string_view s = impl_.stdout_pipe.read();
			stdout_string_ += s;
			break;
		}

		case bit_bucket:
		case inherit:
			break;
	}


	switch (stderr_flags_)
	{
		case forward_to_log:
		{
			std::string_view s = impl_.stderr_pipe.read();

			for_each_line(s, [&](auto&& line)
			{
				filter f = {line, context::std_err, stderr_level_, false};

				if (stderr_filter_)
				{
					stderr_filter_(f);
					if (f.ignore)
						return;
				}

				cx_->log(f.r, f.lv, f.line);
			});

			break;
		}

		case keep_in_string:
		{
			std::string_view s = impl_.stderr_pipe.read();
			stderr_string_ += s;
			break;
		}

		case bit_bucket:
		case inherit:
			break;
	}
}

void process::on_completed()
{
	// one last time
	read_pipes();

	if (impl_.interrupt)
		return;

	if (!GetExitCodeProcess(impl_.handle.get(), &code_))
	{
		const auto e = GetLastError();
		cx_->error(context::cmd, "failed to get exit code", e);
		code_ = 0xffff;
	}

	// success
	if (code_ == 0)
	{
		cx_->trace(context::cmd, "process exit code is 0");
		return;
	}

	if (flags_ & allow_failure)
	{
		cx_->trace(context::cmd,
			"process failed but failure was allowed");
	}
	else
	{
		dump_error_log_file();

		cx_->bail_out(context::cmd,
			make_name() + " returned " + std::to_string(code_));
	}
}

void process::on_timeout(bool& already_interrupted)
{
	read_pipes();

	if (impl_.interrupt && !already_interrupted)
	{
		if (flags_ & terminate_on_interrupt)
		{
			cx_->trace(context::cmd,
				"terminating process (flag is set)");

			::TerminateProcess(impl_.handle.get(), 0xffff);
		}
		else
		{
			const auto pid = GetProcessId(impl_.handle.get());

			if (pid == 0)
			{
				cx_->trace(context::cmd,
					"process id is 0, terminating instead");

				::TerminateProcess(impl_.handle.get(), 0xffff);
			}
			else
			{
				cx_->trace(context::cmd,
					"sending sigint to " + std::to_string(pid));

				GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);
			}
		}

		already_interrupted = true;
	}
}

void process::dump_error_log_file() noexcept
{
	try
	{
		if (error_log_file_.empty())
			return;

		if (fs::exists(error_log_file_))
		{
			std::string log = op::read_text_file(
				*cx_, error_log_file_, op::optional);

			if (log.empty())
				return;

			cx_->error(context::cmd,
				make_name() + " failed, "
				"content of " + error_log_file_.string() + ":");

			for_each_line(log, [&](auto&& line)
			{
				cx_->error(context::cmd, std::string(8, ' ') + std::string(line));
			});
		}
		else
		{
			cx_->debug(context::cmd,
				"external error log file " + error_log_file_.string() + " "
				"doesn't exist");
		}
	}
	catch(...)
	{
		// eat it
	}
}

int process::exit_code() const
{
	return static_cast<int>(code_);
}

std::string process::steal_stdout()
{
	return std::move(stdout_string_);
}

std::string process::steal_stderr()
{
	return std::move(stderr_string_);
}

void process::add_arg(const std::string& k, const std::string& v, arg_flags f)
{
	if ((f & log_debug) && !conf::log_debug())
		return;

	if ((f & log_trace) && !conf::log_trace())
		return;

	if ((f & log_dump) && !conf::log_dump())
		return;

	if ((f & log_quiet) && conf::log_trace())
		return;

	if (k.empty() && v.empty())
		return;

	if (k.empty())
		cmd_ += " " + v;
	else if ((f & nospace) || k.back() == '=')
		cmd_ += " " + k + v;
	else
		cmd_ += " " + k + " " + v;
}

std::string process::arg_to_string(const char* s, bool force_quote)
{
	if (force_quote)
		return "\"" + std::string(s) + "\"";
	else
		return s;
}

std::string process::arg_to_string(const std::string& s, bool force_quote)
{
	if (force_quote)
		return "\"" + std::string(s) + "\"";
	else
		return s;
}

std::string process::arg_to_string(const fs::path& p, bool)
{
	return "\"" + p.string() + "\"";
}

std::string process::arg_to_string(const url& u, bool force_quote)
{
	if (force_quote)
		return "\"" + u.string() + "\"";
	else
		return u.string();
}

}	// namespace

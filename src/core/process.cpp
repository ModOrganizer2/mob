#include "pch.h"
#include "process.h"
#include "conf.h"
#include "context.h"
#include "op.h"
#include "pipe.h"
#include "../net.h"

namespace mob
{

// handle to dev/null
//
HANDLE get_bit_bucket()
{
	SECURITY_ATTRIBUTES sa { .nLength = sizeof(sa), .bInheritHandle = TRUE };
	return ::CreateFileW(L"NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, 0);
}


process::filter::filter(std::string_view line, context::reason r, context::level lv)
	: line(line), r(r), lv(lv), discard(false)
{
}

process::impl::impl(const impl& i)
	: interrupt(i.interrupt.load())
{
}

process::impl& process::impl::operator=(const impl&)
{
	// none of these things should be copied when copying a process object,
	// process should not normally be copied after they've started

	handle = {};
	job = {};
	interrupt = false;
	stdout_pipe = {};
	stderr_pipe = {};
	stdin_pipe = {};

	return *this;
}


process::io::io() :
	unicode(false), chcp(-1),
	out(context::level::trace), err(context::level::error), in_offset(0)
{
}

process::exec::exec()
	: code(0)
{
	// default success exit code is just 0
	success.insert(0);
}


process::process()
	: cx_(&gcx()), flags_(process::noflags)
{
}

// anchors
process::process(process&&) = default;
process::process(const process&) = default;
process& process::operator=(const process&) = default;
process& process::operator=(process&&) = default;

process::~process()
{
	join();
}

process process::raw(const context& cx, const std::string& cmd)
{
	process p;
	p.cx_ = &cx;
	p.exec_.raw = cmd;
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
		return path_to_utf8(exec_.bin.stem());
	else
		return name_;
}

process& process::binary(const fs::path& p)
{
	exec_.bin = p;
	return *this;
}

const fs::path& process::binary() const
{
	return exec_.bin;
}

process& process::cwd(const fs::path& p)
{
	exec_.cwd = p;
	return *this;
}

const fs::path& process::cwd() const
{
	return exec_.cwd;
}

process& process::stdout_flags(stream_flags s)
{
	io_.out.flags = s;
	return *this;
}

process& process::stdout_level(context::level lv)
{
	io_.out.level = lv;
	return *this;
}

process& process::stdout_filter(filter_fun f)
{
	io_.out.filter = f;
	return *this;
}

process& process::stdout_encoding(encodings e)
{
	io_.out.encoding = e;
	return *this;
}

process& process::stderr_flags(stream_flags s)
{
	io_.err.flags = s;
	return *this;
}

process& process::stderr_level(context::level lv)
{
	io_.err.level = lv;
	return *this;
}

process& process::stderr_filter(filter_fun f)
{
	io_.err.filter = f;
	return *this;
}

process& process::stderr_encoding(encodings e)
{
	io_.err.encoding = e;
	return *this;
}

process& process::stdin_string(std::string s)
{
	io_.in = s;
	return *this;
}

process& process::cmd_unicode(bool b)
{
	io_.unicode = b;

	if (b)
	{
		io_.out.encoding = encodings::utf16;
		io_.err.encoding = encodings::utf16;
	}

	return *this;
}

process& process::chcp(int i)
{
	io_.chcp = i;
	return *this;
}

process& process::external_error_log(const fs::path& p)
{
	io_.error_log_file = p;
	return *this;
}

process& process::flags(process_flags f)
{
	flags_ = f;
	return *this;
}

process::process_flags process::flags() const
{
	return flags_;
}

process& process::success_exit_codes(const std::set<int>& v)
{
	exec_.success = v;
	return *this;
}

process& process::env(const mob::env& e)
{
	exec_.env = e;
	return *this;
}

std::string process::make_name() const
{
	if (!name().empty())
		return name();

	return make_cmd();
}

std::string process::make_cmd() const
{
	if (!exec_.raw.empty())
		return exec_.raw;

	// "bin" args...
	return "\"" + path_to_utf8(exec_.bin) + "\"" + exec_.cmd;
}

void process::pipe_into(const process& p)
{
	exec_.raw = make_cmd() + " | " + p.make_cmd();
}

void process::run()
{
	// log cwd
	if (!exec_.cwd.empty())
		cx_->debug(context::cmd, "> cd {}", exec_.cwd);

	const auto what = make_cmd();
	cx_->debug(context::cmd, "> {}", what);

	if (conf::dry())
		return;

	// shouldn't happen
	if (exec_.raw.empty() && exec_.bin.empty())
		cx_->bail_out(context::cmd, "process: nothing to run");

	do_run(what);
}

void process::delete_external_log_file()
{
	if (fs::exists(io_.error_log_file))
	{
		cx_->trace(context::cmd,
			"external error log file {} exists, deleting", io_.error_log_file);

		op::delete_file(*cx_, io_.error_log_file, op::optional);
	}
}

void process::create_job()
{
	SetLastError(0);
	HANDLE job = CreateJobObjectW(nullptr, nullptr);
	const auto e = GetLastError();

	if (job == 0)
	{
		cx_->warning(context::cmd,
			"failed to create job, {}", error_message(e));
	}
	else
	{
		MOB_ASSERT(e != ERROR_ALREADY_EXISTS);
		impl_.job.reset(job);
	}
}

handle_ptr process::redirect_stdout(STARTUPINFOW& si)
{
	handle_ptr h;

	switch (io_.out.flags)
	{
		case forward_to_log:
		case keep_in_string:
		{
			impl_.stdout_pipe.reset(new async_pipe_stdout(*cx_));
			h = impl_.stdout_pipe->create();
			si.hStdOutput = h.get();
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

	return h;
}

handle_ptr process::redirect_stderr(STARTUPINFOW& si)
{
	handle_ptr h;

	switch (io_.err.flags)
	{
		case forward_to_log:
		case keep_in_string:
		{
			impl_.stderr_pipe.reset(new async_pipe_stdout(*cx_));
			h = impl_.stderr_pipe->create();
			si.hStdError = h.get();
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

	return h;
}

handle_ptr process::redirect_stdin(STARTUPINFOW& si)
{
	handle_ptr h;

	if (io_.in)
	{
		impl_.stdin_pipe.reset(new async_pipe_stdin(*cx_));
		h = impl_.stdin_pipe->create();
	}
	else
	{
		h.reset(get_bit_bucket());
	}

	si.hStdInput = h.get();

	return h;
}

void process::do_run(const std::string& what)
{
	delete_external_log_file();
	create_job();

	io_.out.buffer = encoded_buffer(io_.out.encoding);
	io_.err.buffer = encoded_buffer(io_.err.encoding);

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;

	// these handles are given to STARTUPINFOW and must stay alive until the
	// process is created in create(), they can be closed after that
	handle_ptr stdout_handle = redirect_stdout(si);
	handle_ptr stderr_handle = redirect_stderr(si);
	handle_ptr stdin_handle = redirect_stdin(si);

	const std::wstring cmd = utf8_to_utf16(this_env::get("COMSPEC"));
	const std::wstring args = make_cmd_args(what);
	const std::wstring cwd = exec_.cwd.native();

	create(cmd, args, cwd, si);
}

void process::create(
	std::wstring cmd, std::wstring args, std::wstring cwd, STARTUPINFOW si)
{
	cx_->trace(context::cmd, "creating process");

	if (!cwd.empty())
		op::create_directories(*cx_, cwd);

	// cwd
	const wchar_t* cwd_p = (cwd.empty() ? nullptr : cwd.c_str());

	// flags
	const DWORD flags =
		// will forward sigint to child processes
		CREATE_NEW_PROCESS_GROUP |

		// the pointer given for environment variables is a utf16 string, not
		// codepage
		CREATE_UNICODE_ENVIRONMENT;


	// creating process
	PROCESS_INFORMATION pi = {};
	const auto r = ::CreateProcessW(
		cmd.c_str(), args.data(), nullptr, nullptr,
		TRUE, // inherit handles
		flags, exec_.env.get_unicode_pointers(), cwd_p, &si, &pi);


	if (!r)
	{
		const auto e = GetLastError();
		cx_->bail_out(context::cmd,
			"failed to start '{}', {}", args, error_message(e));
	}

	if (impl_.job)
	{
		if (!::AssignProcessToJobObject(impl_.job.get(), pi.hProcess))
		{
			// this shouldn't fail, but the only consequence is that ctrl-c
			// won't be able to kill everything, so make it a warning
			const auto e = GetLastError();
			cx_->warning(context::cmd,
				"can't assign process to job, {}", error_message(e));
		}
	}

	cx_->trace(context::cmd, "pid {}", pi.dwProcessId);

	// not needed
	::CloseHandle(pi.hThread);

	// process handle
	impl_.handle.reset(pi.hProcess);
}

std::wstring process::make_cmd_args(const std::string& what) const
{
	std::wstring s;

	// /U forces cmd builtins to output utf16, such as `set` or `env`, used by
	// vcvars to get the environment variables
	if (io_.unicode)
		s += L"/U ";

	// /C runs the command and exits
	s += L"/C ";


	s += L"\"";

		// run chcp first if necessary
		if (io_.chcp != -1)
			s += L"chcp " + std::to_wstring(io_.chcp) + L" && ";

		// process command line
		s += utf8_to_utf16(what);

	s += L"\"";

	return s;
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

	// remembers if the process was already interrupted
	bool interrupted = false;

	// close the handle quickly after termination
	guard g([&] { impl_.handle = {}; });

	cx_->trace(context::cmd, "joining");

	for (;;)
	{
		// returns if the process is done or after the timeout
		const auto r = WaitForSingleObject(impl_.handle.get(), wait_timeout);

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
			cx_->bail_out(context::cmd,
				"failed to wait on process", error_message(e));
		}
	}

	if (interrupted)
		cx_->trace(context::cmd, "process interrupted and finished");
}

void process::read_pipes(bool finish)
{
	read_pipe(finish, io_.out, *impl_.stdout_pipe, context::std_out);
	read_pipe(finish, io_.err, *impl_.stderr_pipe, context::std_err);
}

void process::read_pipe(
	bool finish, stream& s, async_pipe_stdout& pipe, context::reason r)
{
	switch (s.flags)
	{
		case forward_to_log:
		{
			// read from the pipe, add the bytes to the buffer
			s.buffer.add(pipe.read(finish));

			// for each line in the buffer
			s.buffer.next_utf8_lines(finish, [&](std::string&& line)
			{
				// filter it, if there's a callback
				filter f(line, r, s.level);

				if (s.filter)
				{
					s.filter(f);
					if (f.discard)
						return;
				}

				// remember this log line, can be dumped after the process
				// terminates
				io_.logs[f.lv].emplace_back(std::move(line));

				// don't log when ignore_output_on_success was specified, the
				// process must finish before knowing whether to log or not
				if (!is_set(flags_, ignore_output_on_success))
					cx_->log_string(f.r, f.lv, f.line);

			});

			break;
		}

		case keep_in_string:
		{
			// read from the pipe, add the bytes to the buffer
			s.buffer.add(pipe.read(finish));
			break;
		}

		case bit_bucket:
		case inherit:
		{
			// no-op
			break;
		}
	}
}

void process::on_completed()
{
	// none of this stuff is needed if the process was interrupted, mob will
	// exit shortly
	if (impl_.interrupt)
		return;

	if (!GetExitCodeProcess(impl_.handle.get(), &exec_.code))
	{
		const auto e = GetLastError();

		cx_->error(context::cmd,
			"failed to get exit code, ", error_message(e));

		exec_.code = 0xffff;
	}

	// pipes are finicky, or I just don't understand how they work
	//
	// I've seen empty pipes after processes finish even though there was still
	// data left somewhere in between that hadn't reached this end of the pipe
	// yet
	//
	// so pipes are read one last time with `finish` false, meaning that an
	// empty pipe won't be closed and the last line of the buffer won't be
	// processed yet
	//
	// then pipes are read again in a loop with `finish` true, and hopefully
	// all the content will have been read by that time

	read_pipes(false);

	for (;;)
	{
		read_pipes(true);

		// loop until both pipes are closed
		if (impl_.stdout_pipe->closed() && impl_.stderr_pipe->closed())
			break;
	}


	// check if the exit code is considered success
	if (exec_.success.contains(static_cast<int>(exec_.code)))
		on_process_successful();
	else
		on_process_failed();
}

void process::on_process_successful()
{
	const bool ignore_output = is_set(flags_, ignore_output_on_success);
	const auto& warnings = io_.logs[context::level::warning];
	const auto& errors = io_.logs[context::level::error];

	if (ignore_output || (warnings.empty() && errors.empty()))
	{
		// the process was successful and there were no warnings or errors,
		// or they should be ignored
		cx_->trace(context::cmd,
			"process exit code is {} (considered success)", exec_.code);
	}
	else
	{
		// the process was successful, but there were warnings or errors, log
		// them

		cx_->warning(
			context::cmd,
			"process exit code is {} (considered success), "
			"but stderr had something", exec_.code);

		cx_->warning(context::cmd, "process was: {}", make_cmd());
		cx_->warning(context::cmd, "stderr:");

		for (auto&& line : warnings)
			cx_->warning(context::std_err, "        {}", line);

		for (auto&& line : errors)
			cx_->warning(context::std_err, "        {}", line);
	}
}

void process::on_process_failed()
{
	if (flags_ & allow_failure)
	{
		// ignore failure if the flag is set, it's used for optional things
		cx_->trace(context::cmd,
			"process failed but failure was allowed");
	}
	else
	{
		dump_error_log_file();
		dump_stderr();
		cx_->bail_out(context::cmd, "{} returned {}", make_name(), exec_.code);
	}
}

void process::on_timeout(bool& already_interrupted)
{
	read_pipes(false);
	feed_stdin();

	if (!already_interrupted)
		already_interrupted = check_interrupted();
}

void process::feed_stdin()
{
	if (io_.in && io_.in_offset < io_.in->size())
	{
		io_.in_offset += impl_.stdin_pipe->write({
			io_.in->data() + io_.in_offset, io_.in->size() - io_.in_offset});

		if (io_.in_offset >= io_.in->size())
		{
			impl_.stdin_pipe->close();
			io_.in = {};
		}
	}
}

bool process::check_interrupted()
{
	if (!impl_.interrupt)
		return false;

	const auto pid = GetProcessId(impl_.handle.get());

	// interruption is normally done by sending sigint, which requires a pid;
	// without a pid, the process can be killed from the handle

	if (pid == 0)
	{
		cx_->trace(context::cmd,
			"process id is 0, terminating instead");

		terminate();
	}
	else
	{
		cx_->trace(context::cmd, "sending sigint to {}", pid);
		GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);

		if (flags_ & terminate_on_interrupt)
		{
			// this process doesn't support sigint or doesn't handle it very
			// well; sigint is also sent for good measure

			cx_->trace(context::cmd,
				"terminating process (flag is set)");

			terminate();
		}
	}

	return true;
}

void process::terminate()
{
	UINT exit_code = 0xff;

	if (impl_.job)
	{
		// kill all the child processes in the job

		JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = {};

		const auto r = ::QueryInformationJobObject(
			impl_.job.get(), JobObjectBasicAccountingInformation,
			&info, sizeof(info), nullptr);

		if (r)
		{
			gcx().trace(context::cmd,
				"terminating job, {} processes ({} spawned total)",
				info.ActiveProcesses, info.TotalProcesses);
		}
		else
		{
			gcx().trace(context::cmd, "terminating job");
		}

		if (::TerminateJobObject(impl_.job.get(), exit_code))
		{
			// done
			return;
		}

		const auto e = GetLastError();
		gcx().warning(context::cmd,
			"failed to terminate job, {}", error_message(e));
	}

	// either job creation failed or job termination failed, last ditch attempt
	::TerminateProcess(impl_.handle.get(), exit_code);
}

void process::dump_error_log_file() noexcept
{
	if (io_.error_log_file.empty())
		return;

	if (!fs::exists(io_.error_log_file))
	{
		cx_->debug(context::cmd,
			"external error log file {} doesn't exist", io_.error_log_file);

		return;
	}


	const std::string log = op::read_text_file(
		*cx_, encodings::dont_know, io_.error_log_file, op::optional);

	if (log.empty())
		return;

	cx_->error(context::cmd,
		"{} failed, content of {}:", make_name(), io_.error_log_file);

	for_each_line(log, [&](auto&& line)
	{
		cx_->error(context::cmd, "        {}", line);
	});
}

void process::dump_stderr() noexcept
{
	const std::string s = io_.err.buffer.utf8_string();

	if (s.empty())
	{
		cx_->error(context::cmd,
			"{} failed, stderr was empty", make_name());
	}
	else
	{
		cx_->error(context::cmd,
			"{} failed, {}, content of stderr:", make_name(), make_cmd());

		for_each_line(s, [&](auto&& line)
		{
			cx_->error(context::cmd, "        {}", line);
		});
	}
}

int process::exit_code() const
{
	return static_cast<int>(exec_.code);
}

std::string process::stdout_string()
{
	return io_.out.buffer.utf8_string();
}

std::string process::stderr_string()
{
	return io_.err.buffer.utf8_string();
}

void process::add_arg(const std::string& k, const std::string& v, arg_flags f)
{
	// don't add the argument if it's for a log level that's not active
	if ((f & log_debug) && !context::enabled(context::level::debug))
		return;

	if ((f & log_trace) && !context::enabled(context::level::trace))
		return;

	if ((f & log_dump) && !context::enabled(context::level::dump))
		return;

	if ((f & log_quiet) && context::enabled(context::level::trace))
		return;

	// empty?
	if (k.empty() && v.empty())
		return;

	if (k.empty())
	{
		exec_.cmd += " " + v;
	}
	else
	{
		// key and value, don't put space if the key ends with = or the flag is
		// set
		if ((f & nospace) || k.back() == '=')
			exec_.cmd += " " + k + v;
		else
			exec_.cmd += " " + k + " " + v;
	}
}

std::string process::arg_to_string(const char* s, arg_flags f)
{
	if (f & quote)
		return "\"" + std::string(s) + "\"";
	else
		return s;
}

std::string process::arg_to_string(const std::string& s, arg_flags f)
{
	if (f & quote)
		return "\"" + std::string(s) + "\"";
	else
		return s;
}

std::string process::arg_to_string(const fs::path& p, arg_flags f)
{
	std::string s = path_to_utf8(p);

	if (f & forward_slashes)
		s = replace_all(s, "\\", "/");

	return "\"" + s + "\"";
}

std::string process::arg_to_string(const url& u, arg_flags f)
{
	if (f & quote)
		return "\"" + u.string() + "\"";
	else
		return u.string();
}

std::string process::arg_to_string(int i, arg_flags)
{
	return std::to_string(i);
}

}	// namespace

#include "pch.h"
#include "process.h"
#include "conf.h"
#include "context.h"
#include "op.h"
#include "pipe.h"
#include "../net.h"

namespace mob
{

HANDLE get_bit_bucket()
{
	SECURITY_ATTRIBUTES sa { .nLength = sizeof(sa), .bInheritHandle = TRUE };
	return ::CreateFileW(L"NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, 0);
}


process::impl::impl(const impl& i)
	: interrupt(i.interrupt.load())
{
}

process::impl& process::impl::operator=(const impl& i)
{
	handle = {};
	job = {};
	interrupt = i.interrupt.load();
	stdout_pipe = {};
	stderr_pipe = {};
	stdin_pipe = {};
	stdin_handle = {};

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

process& process::flags(flags_t f)
{
	flags_ = f;
	return *this;
}

process::flags_t process::flags() const
{
	return flags_;
}

process& process::success_exit_codes(std::set<int> v)
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
	if (!name_.empty())
		return name_;

	return make_cmd();
}

std::string process::make_cmd() const
{
	if (!exec_.raw.empty())
		return exec_.raw;

	return "\"" + path_to_utf8(exec_.bin) + "\"" + exec_.cmd;
}

void process::pipe_into(const process& p)
{
	exec_.raw = make_cmd() + " | " + p.make_cmd();
}

void process::run()
{
	if (!exec_.cwd.empty())
		cx_->debug(context::cmd, "> cd {}", exec_.cwd);

	const auto what = make_cmd();
	cx_->debug(context::cmd, "> {}", what);

	if (conf::dry())
		return;

	do_run(what);
}

void process::do_run(const std::string& what)
{
	if (exec_.raw.empty() && exec_.bin.empty())
		cx_->bail_out(context::cmd, "process: nothing to run");

	if (fs::exists(io_.error_log_file))
	{
		cx_->trace(context::cmd,
			"external error log file {} exists, deleting", io_.error_log_file);

		op::delete_file(*cx_, io_.error_log_file, op::optional);
	}


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


	io_.out.buffer = encoded_buffer(io_.out.encoding);
	io_.err.buffer = encoded_buffer(io_.err.encoding);

	STARTUPINFOW si = { .cb=sizeof(si) };
	PROCESS_INFORMATION pi = {};

	handle_ptr stdout_pipe, stderr_pipe;

	impl_.stdout_pipe.reset(new async_pipe_stdout(*cx_));
	impl_.stderr_pipe.reset(new async_pipe_stdout(*cx_));

	switch (io_.out.flags)
	{
		case forward_to_log:
		case keep_in_string:
		{
			stdout_pipe = impl_.stdout_pipe->create();
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

	switch (io_.err.flags)
	{
		case forward_to_log:
		case keep_in_string:
		{
			stderr_pipe = impl_.stderr_pipe->create();
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

	if (io_.in)
	{
		impl_.stdin_pipe.reset(new async_pipe_stdin(*cx_));
		impl_.stdin_handle = impl_.stdin_pipe->create();
	}
	else
	{
		impl_.stdin_handle.reset(get_bit_bucket());
	}

	si.hStdInput = impl_.stdin_handle.get();
	si.dwFlags = STARTF_USESTDHANDLES;

	const std::wstring cmd = utf8_to_utf16(this_env::get("COMSPEC"));
	std::wstring args = make_cmd_args(what);

	const wchar_t* cwd_p = nullptr;
	std::wstring cwd_s;

	if (!exec_.cwd.empty())
	{
		if (!fs::exists(exec_.cwd))
			op::create_directories(*cx_, exec_.cwd);

		cwd_s = exec_.cwd.native();
		cwd_p = (cwd_s.empty() ? nullptr : cwd_s.c_str());
	}

	cx_->trace(context::cmd, "creating process");

	const auto r = ::CreateProcessW(
		cmd.c_str(), args.data(),
		nullptr, nullptr, TRUE,
		CREATE_NEW_PROCESS_GROUP|CREATE_UNICODE_ENVIRONMENT,
		exec_.env.get_unicode_pointers(), cwd_p, &si, &pi);

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
			const auto e = GetLastError();
			cx_->warning(context::cmd,
				"can't assign process to job, {}", error_message(e));
		}
	}

	cx_->trace(context::cmd, "pid {}", pi.dwProcessId);

	::CloseHandle(pi.hThread);
	impl_.handle.reset(pi.hProcess);
}

std::wstring process::make_cmd_args(const std::string& what) const
{
	std::wstring s;

	if (io_.unicode)
		s += L"/U ";

	s += L"/C \"";

	if (io_.chcp != -1)
		s += L"chcp " + std::to_wstring(io_.chcp) + L" && ";

	s += utf8_to_utf16(what) + L"\"";

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

	bool interrupted = false;
	guard g([&] { impl_.handle = {}; });

	cx_->trace(context::cmd, "joining");

	for (;;)
	{
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
			s.buffer.add(pipe.read(finish));

			s.buffer.next_utf8_lines(finish, [&](std::string&& line)
			{
				filter f(line, r, s.level, false);

				if (s.filter)
				{
					s.filter(f);
					if (f.ignore)
						return;
				}

				if (!is_set(flags_, ignore_output_on_success))
					cx_->log_string(f.r, f.lv, f.line);

				io_.logs[f.lv].emplace_back(std::move(line));
			});

			break;
		}

		case keep_in_string:
		{
			s.buffer.add(pipe.read(finish));
			break;
		}

		case bit_bucket:
		case inherit:
			break;
	}
}

void process::on_completed()
{
	if (impl_.interrupt)
		return;

	if (!GetExitCodeProcess(impl_.handle.get(), &exec_.code))
	{
		const auto e = GetLastError();

		cx_->error(context::cmd,
			"failed to get exit code, ", error_message(e));

		exec_.code = 0xffff;
	}

	read_pipes(false);

	for (;;)
	{
		read_pipes(true);

		if (impl_.stdout_pipe->closed() && impl_.stderr_pipe->closed())
			break;
	}

	// success
	if (exec_.success.contains(static_cast<int>(exec_.code)))
	{
		const bool ignore_output = is_set(flags_, ignore_output_on_success);
		const auto& warnings = io_.logs[context::level::warning];
		const auto& errors = io_.logs[context::level::error];

		if (ignore_output || (warnings.empty() && errors.empty()))
		{
			cx_->trace(context::cmd,
				"process exit code is {} (considered success)", exec_.code);
		}
		else
		{
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
		dump_stderr();
		cx_->bail_out(context::cmd, "{} returned {}", make_name(), exec_.code);
	}
}

void process::on_timeout(bool& already_interrupted)
{
	read_pipes(false);

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

	if (impl_.interrupt && !already_interrupted)
	{
		const auto pid = GetProcessId(impl_.handle.get());

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
				cx_->trace(context::cmd,
					"terminating process (flag is set)");

				terminate();
			}
		}

		already_interrupted = true;
	}
}

void process::terminate()
{
	UINT exit_code = 0xff;

	if (impl_.job)
	{
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

	::TerminateProcess(impl_.handle.get(), exit_code);
}

void process::dump_error_log_file() noexcept
{
	if (io_.error_log_file.empty())
		return;

	if (fs::exists(io_.error_log_file))
	{
		std::string log = op::read_text_file(
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
	else
	{
		cx_->debug(context::cmd,
			"external error log file {} doesn't exist", io_.error_log_file);
	}
}

void process::dump_stderr() noexcept
{
	const std::string s = io_.err.buffer.utf8_string();

	if (!s.empty())
	{
		cx_->error(context::cmd,
			"{} failed, {}, content of stderr:", make_name(), make_cmd());

		for_each_line(s, [&](auto&& line)
		{
			cx_->error(context::cmd, "        {}", line);
		});
	}
	else
	{
		cx_->error(context::cmd,
			"{} failed, stderr was empty", make_name());
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
	if ((f & log_debug) && !context::enabled(context::level::debug))
		return;

	if ((f & log_trace) && !context::enabled(context::level::trace))
		return;

	if ((f & log_dump) && !context::enabled(context::level::dump))
		return;

	if ((f & log_quiet) && context::enabled(context::level::trace))
		return;

	if (k.empty() && v.empty())
		return;

	if (k.empty())
		exec_.cmd += " " + v;
	else if ((f & nospace) || k.back() == '=')
		exec_.cmd += " " + k + v;
	else
		exec_.cmd += " " + k + " " + v;
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

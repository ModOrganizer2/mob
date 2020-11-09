#include "pch.h"
#include "process.h"
#include "conf.h"
#include "context.h"
#include "op.h"
#include "../net.h"

namespace mob
{

const DWORD wait_timeout = 50;
static std::atomic<int> g_next_pipe_id(0);


HANDLE get_bit_bucket()
{
	SECURITY_ATTRIBUTES sa { .nLength = sizeof(sa), .bInheritHandle = TRUE };
	return ::CreateFileW(L"NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, 0);
}

async_pipe::async_pipe(const context& cx)
	: cx_(cx), pending_(false), closed_(true)
{
	buffer_ = std::make_unique<char[]>(buffer_size);
	std::memset(buffer_.get(), 0, buffer_size);

	std::memset(&ov_, 0, sizeof(ov_));
}

bool async_pipe::closed() const
{
	return closed_;
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
		cx_.bail_out(context::cmd,
			"CreateEvent failed, {}", error_message(e));
	}

	event_.reset(ov_.hEvent);
	closed_ = false;

	return out;
}

std::string_view async_pipe::read(bool finish)
{
	std::string_view s;

	if (closed_)
		return s;

	if (pending_)
		s = check_pending();
	else
		s = try_read();

	if (finish && s.empty())
	{
		::CancelIo(stdout_.get());
		closed_ = true;
	}

	return s;
}

HANDLE async_pipe::create_pipe()
{
	const auto pipe_id = g_next_pipe_id.fetch_add(1) + 1;

	const std::wstring pipe_name =
		LR"(\\.\pipe\mob_pipe)" + std::to_wstring(pipe_id);

	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;

	handle_ptr pipe;

	// creating pipe
	{
		HANDLE pipe_handle = ::CreateNamedPipeW(
			pipe_name.c_str(),
			PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
			PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,
			1, buffer_size, buffer_size, wait_timeout, &sa);

		if (pipe_handle == INVALID_HANDLE_VALUE)
		{
			const auto e = GetLastError();
			cx_.bail_out(context::cmd,
				"CreateNamedPipeW failed, {}", error_message(e));
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
			cx_.bail_out(context::cmd,
				"DuplicateHandle for pipe failed, {}", error_message(e));
		}

		stdout_.reset(output_read);
	}


	// creating handle to pipe which is passed to CreateProcess()
	HANDLE output_write = ::CreateFileW(
		pipe_name.c_str(), FILE_WRITE_DATA|SYNCHRONIZE, 0,
		&sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (output_write == INVALID_HANDLE_VALUE)
	{
		const auto e = GetLastError();
		cx_.bail_out(context::cmd,
			"CreateFileW for pipe failed, {}", error_message(e));
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
				// broken pipe means the process is finished
				closed_ = true;
				break;
			}

			default:
			{
				cx_.bail_out(context::cmd,
					"async_pipe read failed, {}", error_message(e));
				break;
			}
		}

		return {};
	}

	MOB_ASSERT(bytes_read <= buffer_size);

	return {buffer_.get(), bytes_read};
}

std::string_view async_pipe::check_pending()
{
	DWORD bytes_read = 0;

	const auto r = WaitForSingleObject(event_.get(), wait_timeout);

	if (r == WAIT_FAILED) {
		const auto e = GetLastError();
		cx_.bail_out(context::cmd,
			"WaitForSingleObject in async_pipe failed, {}", error_message(e));
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
				// broken pipe means the process is finished
				closed_ = true;
				break;
			}

			default:
			{
				cx_.bail_out(context::cmd,
					"GetOverlappedResult failed in async_pipe, {}",
					error_message(e));

				break;
			}
		}

		return {};
	}

	MOB_ASSERT(bytes_read <= buffer_size);

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
	handle = {};
	job = {};
	interrupt = i.interrupt.load();
	stdout_pipe = {};
	stderr_pipe = {};

	return *this;
}


process::process() :
	cx_(&gcx()), unicode_(false), chcp_(-1), flags_(process::noflags),
	stdout_(context::level::trace), stderr_(context::level::error), code_(0)
{
	success_.insert(0);
}

process::~process()
{
	join();
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
		return path_to_utf8(bin_.stem());
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
	stdout_.flags = s;
	return *this;
}

process& process::stdout_level(context::level lv)
{
	stdout_.level = lv;
	return *this;
}

process& process::stdout_filter(filter_fun f)
{
	stdout_.filter = f;
	return *this;
}

process& process::stdout_encoding(encodings e)
{
	stdout_.encoding = e;
	return *this;
}

process& process::stderr_flags(stream_flags s)
{
	stderr_.flags = s;
	return *this;
}

process& process::stderr_level(context::level lv)
{
	stderr_.level = lv;
	return *this;
}

process& process::stderr_filter(filter_fun f)
{
	stderr_.filter = f;
	return *this;
}

process& process::stderr_encoding(encodings e)
{
	stderr_.encoding = e;
	return *this;
}

process& process::cmd_unicode(bool b)
{
	unicode_ = b;

	if (b)
	{
		stdout_.encoding = encodings::utf16;
		stderr_.encoding = encodings::utf16;
	}

	return *this;
}

process& process::chcp(int i)
{
	chcp_ = i;
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

process& process::success_exit_codes(std::set<int> v)
{
	success_ = v;
	return *this;
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

	return "\"" + path_to_utf8(bin_) + "\"" + cmd_;
}

void process::pipe_into(const process& p)
{
	raw_ = make_cmd() + " | " + p.make_cmd();
}

void process::run()
{
	if (!cwd_.empty())
		cx_->debug(context::cmd, "> cd {}", cwd_);

	const auto what = make_cmd();
	cx_->debug(context::cmd, "> {}", what);

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
			"external error log file {} exists, deleting", error_log_file_);

		op::delete_file(*cx_, error_log_file_, op::optional);
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


	stdout_.buffer = encoded_buffer(stdout_.encoding);
	stderr_.buffer = encoded_buffer(stderr_.encoding);

	STARTUPINFOW si = { .cb=sizeof(si) };
	PROCESS_INFORMATION pi = {};

	handle_ptr stdout_pipe, stderr_pipe, stdin_pipe;

	impl_.stdout_pipe.reset(new async_pipe(*cx_));
	impl_.stderr_pipe.reset(new async_pipe(*cx_));

	switch (stdout_.flags)
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

	switch (stderr_.flags)
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

	stdin_pipe.reset(get_bit_bucket());
	si.hStdInput = stdin_pipe.get();

	si.dwFlags = STARTF_USESTDHANDLES;

	const std::wstring cmd = utf8_to_utf16(this_env::get("COMSPEC"));
	std::wstring args = make_cmd_args(what);

	const wchar_t* cwd_p = nullptr;
	std::wstring cwd_s;

	if (!cwd_.empty())
	{
		if (!fs::exists(cwd_))
			op::create_directories(*cx_, cwd_);

		cwd_s = cwd_.native();
		cwd_p = (cwd_s.empty() ? nullptr : cwd_s.c_str());
	}

	cx_->trace(context::cmd, "creating process");

	const auto r = ::CreateProcessW(
		cmd.c_str(), args.data(),
		nullptr, nullptr, TRUE,
		CREATE_NEW_PROCESS_GROUP|CREATE_UNICODE_ENVIRONMENT,
		env_.get_unicode_pointers(), cwd_p, &si, &pi);

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

	if (unicode_)
		s += L"/U ";

	s += L"/C \"";

	if (chcp_ != -1)
		s += L"chcp " + std::to_wstring(chcp_) + L" && ";

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
	read_pipe(finish, stdout_, *impl_.stdout_pipe, context::std_out);
	read_pipe(finish, stderr_, *impl_.stderr_pipe, context::std_err);
}

void process::read_pipe(
	bool finish, stream& s, async_pipe& pipe, context::reason r)
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

				logs_[f.lv].emplace_back(std::move(line));
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

	if (!GetExitCodeProcess(impl_.handle.get(), &code_))
	{
		const auto e = GetLastError();

		cx_->error(context::cmd,
			"failed to get exit code, ", error_message(e));

		code_ = 0xffff;
	}

	read_pipes(false);

	for (;;)
	{
		read_pipes(true);

		if (impl_.stdout_pipe->closed() && impl_.stderr_pipe->closed())
			break;
	}

	// success
	if (success_.contains(static_cast<int>(code_)))
	{
		const bool ignore_output = is_set(flags_, ignore_output_on_success);
		const auto& warnings = logs_[context::level::warning];
		const auto& errors = logs_[context::level::error];

		if (ignore_output || (warnings.empty() && errors.empty()))
		{
			cx_->trace(context::cmd,
				"process exit code is {} (considered success)", code_);
		}
		else
		{
			cx_->warning(
				context::cmd,
				"process exit code is {} (considered success), "
				"but stderr had something", code_);

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
		cx_->bail_out(context::cmd, "{} returned {}", make_name(), code_);
	}
}

void process::on_timeout(bool& already_interrupted)
{
	read_pipes(false);

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
	if (error_log_file_.empty())
		return;

	if (fs::exists(error_log_file_))
	{
		std::string log = op::read_text_file(
			*cx_, encodings::dont_know, error_log_file_, op::optional);

		if (log.empty())
			return;

		cx_->error(context::cmd,
			"{} failed, content of {}:", make_name(), error_log_file_);

		for_each_line(log, [&](auto&& line)
		{
			cx_->error(context::cmd, "        {}", line);
		});
	}
	else
	{
		cx_->debug(context::cmd,
			"external error log file {} doesn't exist", error_log_file_);
	}
}

void process::dump_stderr() noexcept
{
	const std::string s = stderr_.buffer.utf8_string();

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
	return static_cast<int>(code_);
}

std::string process::stdout_string()
{
	return stdout_.buffer.utf8_string();
}

std::string process::stderr_string()
{
	return stderr_.buffer.utf8_string();
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
		cmd_ += " " + v;
	else if ((f & nospace) || k.back() == '=')
		cmd_ += " " + k + v;
	else
		cmd_ += " " + k + " " + v;
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


encoded_buffer::encoded_buffer(encodings e, std::string bytes)
	: e_(e), bytes_(std::move(bytes)), last_(0)
{
}

void encoded_buffer::add(std::string_view bytes)
{
	bytes_.append(bytes.begin(), bytes.end());
}

std::string encoded_buffer::utf8_string() const
{
	return bytes_to_utf8(e_, bytes_);
}

}	// namespace

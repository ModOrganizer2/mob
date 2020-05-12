#include "pch.h"
#include "process.h"
#include "conf.h"
#include "net.h"
#include "context.h"
#include "op.h"

namespace mob
{

const DWORD pipe_timeout = 50;
const DWORD process_wait_timeout = 50;


HANDLE get_bit_bucket()
{
	SECURITY_ATTRIBUTES sa { .nLength = sizeof(sa), .bInheritHandle = TRUE };
	return ::CreateFileW(L"NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, 0);
}

async_pipe::async_pipe()
	:  pending_(false), closed_(true)
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
		bail_out("CreateEvent failed", error_message(e));
	}

	event_.reset(ov_.hEvent);
	closed_ = false;

	return out;
}

std::string_view async_pipe::read()
{
	if (closed_)
		return {};

	if (pending_)
		return check_pending();
	else
		return try_read();
}

HANDLE async_pipe::create_pipe()
{
	static std::atomic<int> pipe_id(0);

	const std::wstring pipe_name =
		L"\\\\.\\pipe\\mob_pipe" + std::to_wstring(++pipe_id);

	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;

	handle_ptr pipe;

	// creating pipe
	{
		HANDLE pipe_handle = ::CreateNamedPipeW(
			pipe_name.c_str(), PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,
			1, buffer_size, buffer_size, pipe_timeout, &sa);

		if (pipe_handle == INVALID_HANDLE_VALUE)
		{
			const auto e = GetLastError();
			bail_out("CreateNamedPipeW failed", error_message(e));
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
			bail_out("DuplicateHandle for pipe", error_message(e));
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
		bail_out("CreateFileW for pipe failed", error_message(e));
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
				bail_out("async_pipe read failed", error_message(e));
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

	const auto r = WaitForSingleObject(event_.get(), pipe_timeout);

	if (r == WAIT_FAILED) {
		const auto e = GetLastError();
		bail_out("WaitForSingleObject in async_pipe failed", error_message(e));
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
				bail_out(
					"GetOverlappedResult failed in async_pipe",
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
	interrupt = i.interrupt.load();
	return *this;
}


process::process()
	: cx_(&gcx()), unicode_(false), chcp_(-1), flags_(process::noflags), code_(0)
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

	stdout_.buffer = encoded_buffer(stdout_.encoding);
	stderr_.buffer = encoded_buffer(stderr_.encoding);

	STARTUPINFOW si = { .cb=sizeof(si) };
	PROCESS_INFORMATION pi = {};

	handle_ptr stdout_pipe, stderr_pipe, stdin_pipe;

	switch (stdout_.flags)
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

	switch (stderr_.flags)
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

	stdin_pipe.reset(get_bit_bucket());
	si.hStdInput = stdin_pipe.get();

	si.dwFlags = STARTF_USESTDHANDLES;

	const std::wstring cmd = utf8_to_utf16(this_env::get("COMSPEC"));
	std::wstring args = make_cmd_args(what);

	const wchar_t* cwd_p = nullptr;
	std::wstring cwd_s;

	if (!cwd_.empty())
	{
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
		const auto r = WaitForSingleObject(
			impl_.handle.get(), process_wait_timeout);

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
	read_pipe(finish, stdout_, impl_.stdout_pipe, context::std_out);
	read_pipe(finish, stderr_, impl_.stderr_pipe, context::std_err);
}

void process::read_pipe(
	bool finish, stream& s, async_pipe& pipe, context::reason r)
{
	switch (s.flags)
	{
		case forward_to_log:
		{
			s.buffer.add(pipe.read());

			s.buffer.next_utf8_lines(finish, [&](auto&& line)
			{
				filter f = {line, r, s.level, false};

				if (s.filter)
				{
					s.filter(f);
					if (f.ignore)
						return;
				}

				cx_->log(f.r, f.lv, "{}", f.line);
			});

			break;
		}

		case keep_in_string:
		{
			s.buffer.add(pipe.read());
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
	for (;;)
	{
		read_pipes(false);

		if (impl_.stdout_pipe.closed() && impl_.stderr_pipe.closed())
			break;
	}

	read_pipes(true);


	if (impl_.interrupt)
		return;

	if (!GetExitCodeProcess(impl_.handle.get(), &code_))
	{
		const auto e = GetLastError();

		cx_->error(context::cmd,
			"failed to get exit code, ", error_message(e));

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
		dump_stderr();
		cx_->bail_out(context::cmd, "{} returned {}", make_name(), code_);
	}
}

void process::on_timeout(bool& already_interrupted)
{
	read_pipes(false);

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
				cx_->trace(context::cmd, "sending sigint to {}", pid);
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
	catch(...)
	{
		// eat it
	}
}

void process::dump_stderr() noexcept
{
	try
	{
		const std::string s = stderr_.buffer.utf8_string();

		if (!s.empty())
		{
			cx_->error(context::cmd,
				"{} failed, content of stderr:", make_name());

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
	catch(...)
	{
		// eat it
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
	return "\"" + path_to_utf8(p) + "\"";
}

std::string process::arg_to_string(const url& u, bool force_quote)
{
	if (force_quote)
		return "\"" + u.string() + "\"";
	else
		return u.string();
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

template <class CharT>
std::basic_string<CharT> next_line(
	bool finished, std::string_view bytes, std::size_t& byte_offset)
{
	std::size_t size = bytes.size();
	if ((size & 1) == 1)
		--size;

	const CharT* start = reinterpret_cast<const CharT*>(bytes.data() + byte_offset);
	const CharT* end = reinterpret_cast<const CharT*>(bytes.data() + size);
	const CharT* p = start;

	std::basic_string<CharT> line;

	while (p != end)
	{
		if (*p == CharT('\n') || *p == CharT('\r'))
		{
			line.assign(start, static_cast<std::size_t>(p - start));

			while (p != end && (*p == CharT('\n') || *p == CharT('\r')))
				++p;

			if (!line.empty())
				break;

			start = p;
		}
		else
		{
			++p;
		}
	}

	if (line.empty() && finished)
	{
		line = {
			reinterpret_cast<const wchar_t*>(bytes.data() + byte_offset),
			reinterpret_cast<const wchar_t*>(bytes.data() + size)
		};

		byte_offset = bytes.size();
	}
	else
	{
		byte_offset = static_cast<std::size_t>(
			reinterpret_cast<const char*>(p) - bytes.data());

		MOB_ASSERT(byte_offset <= bytes.size());
	}

	return line;
}

std::string encoded_buffer::next_utf8_line(bool finished)
{
	switch (e_)
	{
		case encodings::utf16:
		{
			const std::wstring utf16 = next_line<wchar_t>(finished, bytes_, last_);
			return utf16_to_utf8(utf16);
		}

		case encodings::acp:
		case encodings::oem:
		{
			const std::string cp = next_line<char>(finished, bytes_, last_);
			return bytes_to_utf8(e_, cp);
		}

		case encodings::utf8:
		case encodings::dont_know:
		default:
		{
			return next_line<char>(finished, bytes_, last_);
		}
	}
}

}	// namespace

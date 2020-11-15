#include "pch.h"
#include "pipe.h"
#include "context.h"
#include "process.h"

namespace mob
{

static std::atomic<int> g_next_pipe_id(0);


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

handle_ptr async_pipe::create_for_stdout()
{
	return create(true);
}

handle_ptr async_pipe::create_for_stdin()
{
	return create(false);
}

handle_ptr async_pipe::create(bool for_stdout)
{
	// creating pipe
	handle_ptr out(for_stdout ? create_named_pipe() : create_anonymous_pipe());
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

std::size_t async_pipe::write(std::string_view s)
{
	const DWORD n = static_cast<DWORD>(s.size());
	DWORD written = 0;
	const auto r = ::WriteFile(stdout_.get(), s.data(), n, &written, nullptr);

	if (written >= s.size())
		stdout_ = {};

	return written;
}

HANDLE async_pipe::create_named_pipe()
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
			1, buffer_size, buffer_size, process::wait_timeout, &sa);

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

HANDLE async_pipe::create_anonymous_pipe()
{
	SECURITY_ATTRIBUTES saAttr = {};
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;

	// Create a pipe for the child process's STDIN.
	HANDLE read_pipe, write_pipe;
	if (!CreatePipe(&read_pipe, &write_pipe, &saAttr, 0))
	{
		const auto e = GetLastError();
		cx_.bail_out(context::cmd,
			"CreatePipe failed, {}", error_message(e));
	}

	// Ensure the write handle to the pipe for STDIN is not inherited.
	if (!SetHandleInformation(write_pipe, HANDLE_FLAG_INHERIT, 0))
	{
		const auto e = GetLastError();
		cx_.bail_out(context::cmd,
			"SetHandleInformation failed, {}", error_message(e));
	}

	stdout_.reset(write_pipe);

	return read_pipe;
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

	const auto r = WaitForSingleObject(event_.get(), process::wait_timeout);

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

}	// namespace

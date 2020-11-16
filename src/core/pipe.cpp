#include "pch.h"
#include "pipe.h"
#include "context.h"
#include "process.h"

namespace mob
{

// many processes may be started simultaneously, this is incremented each time
// a pipe is created to make sure pipe names are unique
static std::atomic<int> g_next_pipe_id(0);


async_pipe_stdout::async_pipe_stdout(const context& cx)
	: cx_(cx), pending_(false), closed_(true)
{
	buffer_ = std::make_unique<char[]>(buffer_size);

	std::memset(buffer_.get(), 0, buffer_size);
	std::memset(&ov_, 0, sizeof(ov_));
}

bool async_pipe_stdout::closed() const
{
	return closed_;
}

handle_ptr async_pipe_stdout::create()
{
	// creating pipe
	handle_ptr out(create_named_pipe());
	if (out.get() == INVALID_HANDLE_VALUE)
		return {};

	// creating event
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

std::string_view async_pipe_stdout::read(bool finish)
{
	std::string_view s;

	if (closed_)
	{
		// no-op
		return s;
	}

	if (pending_)
	{
		// the last read() call started an async operation, check if it's done
		s = check_pending();
	}
	else
	{
		// there's no async operation in progress, try to read
		s = try_read();
	}

	if (finish && s.empty())
	{
		// nothing was read from the pipe and `finish` is true, so the process
		// has terminated; in this case, assume the pipe is empty and everything
		// has been read

		// even if the pipe is empty and nothing more is available, try_read()
		// may have started an async operation that would return no data in the
		// future
		//
		// make sure that operation is cancelled, because the kernel would try
		// to use the OVERLAPPED buffer, which will probably have been destroyed
		// by that time
		::CancelIo(pipe_.get());

		// a future call to read() will be a no-op and closed() will return true
		closed_ = true;
	}

	// the bytes that were read, if any
	return s;
}

HANDLE async_pipe_stdout::create_named_pipe()
{
	// unique name
	const auto pipe_id = g_next_pipe_id.fetch_add(1) + 1;

	const std::wstring pipe_name =
		LR"(\\.\pipe\mob_pipe)" + std::to_wstring(pipe_id);


	// creating pipe
	{
		const DWORD open_flags =
			PIPE_ACCESS_INBOUND |          // the pipe will be read from
			FILE_FLAG_OVERLAPPED |         // non blocking
			FILE_FLAG_FIRST_PIPE_INSTANCE; // the pipe must not exist

		// pipes support either bytes or messages, use bytes
		const DWORD mode_flags = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE;

		HANDLE pipe_handle = ::CreateNamedPipeW(
			pipe_name.c_str(), open_flags, mode_flags,
			1, buffer_size, buffer_size, process::wait_timeout, nullptr);

		if (pipe_handle == INVALID_HANDLE_VALUE)
		{
			const auto e = GetLastError();
			cx_.bail_out(context::cmd,
				"CreateNamedPipeW failed, {}", error_message(e));
		}

		pipe_.reset(pipe_handle);
	}


	// creating the write-only end of the pipe that will be passed to
	// CreateProcess()
	SECURITY_ATTRIBUTES sa = {};
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);

	// mark this handle as being inherited by the process; if this is not TRUE,
	// the connection between the two ends of the pipe will be broken
	sa.bInheritHandle = TRUE;

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

std::string_view async_pipe_stdout::try_read()
{
	DWORD bytes_read = 0;

	// read bytes from the pipe
	const auto r = ::ReadFile(
		pipe_.get(), buffer_.get(), buffer_size, &bytes_read, &ov_);

	if (r)
	{
		// some bytes were read, so ReadFile() turned out to be a synchronous
		// operation; this happens when there's already stuff in the pipe
		MOB_ASSERT(bytes_read <= buffer_size);
		return {buffer_.get(), bytes_read};
	}


	// ReadFile() failed, but it's not necessarily an error

	const auto e = GetLastError();

	switch (e)
	{
		case ERROR_IO_PENDING:
		{
			// an async operation was started by the kernel, future calls to
			// read() will end up in check_pending() to see if it completed
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
			// some other hard error
			cx_.bail_out(context::cmd,
				"async_pipe_stdout read failed, {}", error_message(e));
			break;
		}
	}

	// nothing available
	return {};
}

std::string_view async_pipe_stdout::check_pending()
{
	// check if the async operation finished, wait for a short amount of time
	const auto wr = WaitForSingleObject(event_.get(), process::wait_timeout);

	if (wr == WAIT_TIMEOUT)
	{
		// nothing's available
		return {};
	}
	else if (wr == WAIT_FAILED)
	{
		// hard error
		const auto e = GetLastError();
		cx_.bail_out(context::cmd,
			"WaitForSingleObject in async_pipe_stdout failed, {}",
			error_message(e));
	}


	// the operation seems to be finished

	DWORD bytes_read = 0;

	// getting status of the async read operation
	const auto r = ::GetOverlappedResult(pipe_.get(), &ov_, &bytes_read, FALSE);

	if (r)
	{
		// the operation has completed

		// reset for the next read()
		::ResetEvent(event_.get());
		pending_ = false;

		// return the data, if any
		MOB_ASSERT(bytes_read <= buffer_size);
		return {buffer_.get(), bytes_read};
	}

	// GetOverlappedResult() failed, but it's not necessarily an error

	const auto e = GetLastError();

	switch (e)
	{
		case ERROR_IO_INCOMPLETE:
		case WAIT_TIMEOUT:
		{
			// still not completed
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
			// some other hard error
			cx_.bail_out(context::cmd,
				"GetOverlappedResult failed in async_pipe_stdout, {}",
				error_message(e));

			break;
		}
	}

	return {};
}


async_pipe_stdin::async_pipe_stdin(const context& cx)
	: cx_(cx)
{
}

handle_ptr async_pipe_stdin::create()
{
	// this pipe has two ends:
	// - write_pipe is this end of the pipe, it will be written to
	// - read_pipe is the process's end of the pipe, it will be read from
	//
	// read_pipe is given to the process in CreateProcess() and it needs to be
	// inheritable or the connection between both ends of the pipe will be
	// broken
	//
	// write_pipe does not need to be inheritable


	SECURITY_ATTRIBUTES saAttr = {};
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);

	// set both ends of the pipe as inheritable, write_pipe will be changed
	// after
	saAttr.bInheritHandle = TRUE;

	// creating pipe
	HANDLE read_pipe, write_pipe;
	if (!CreatePipe(&read_pipe, &write_pipe, &saAttr, 0))
	{
		const auto e = GetLastError();
		cx_.bail_out(context::cmd,
			"CreatePipe failed, {}", error_message(e));
	}

	// write_pipe does not need to be inheritable, it's not given to the process
	if (!SetHandleInformation(write_pipe, HANDLE_FLAG_INHERIT, 0))
	{
		const auto e = GetLastError();
		cx_.bail_out(context::cmd,
			"SetHandleInformation failed, {}", error_message(e));
	}

	// keep the end that's written to
	pipe_.reset(write_pipe);

	// give to other end to the new process
	return handle_ptr(read_pipe);
}

std::size_t async_pipe_stdin::write(std::string_view s)
{
	// bytes to write
	const DWORD n = static_cast<DWORD>(s.size());

	// bytes actually written
	DWORD written = 0;

	// send to bytes down the pipe
	const auto r = ::WriteFile(pipe_.get(), s.data(), n, &written, nullptr);

	if (!r)
	{
		// hard error
		const auto e = GetLastError();

		cx_.bail_out(context::cmd,
			"WriteFile failed in async_pipe_stdin, {}",
			error_message(e));
	}

	// some bytes were written correctly
	return written;
}

void async_pipe_stdin::close()
{
	pipe_ = {};
}

}	// namespace

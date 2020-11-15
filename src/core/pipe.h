#pragma once

#include "../utility.h"

namespace mob
{

// a pipe connected to a process's stdout or stderr, it is read from
//
class async_pipe_stdout
{
public:
	async_pipe_stdout(const context& cx);

	// a pipe has two ends: one that's given to the process so it can write to
	// it, and another that's kept so it can be read from
	//
	// this creates both ends and returns the handle that should be given to
	// the process
	//
	handle_ptr create();

	// reads from the pipe and returns bytes, if any
	//
	// if `finish` is true (happens when the process has terminated) and nothing
	// is available in the pipe, closed() will return true
	//
	// this may start an async read, so read() must be called repeatedly until
	// the process terminates
	//
	std::string_view read(bool finish);

	// if this returns true, everything has been read from the pipe
	//
	bool closed() const;

private:
	// the maximum number of bytes that can be put in the pipe
	static const std::size_t buffer_size = 50'000;

	// calling context, used for logging
	const context& cx_;

	// end of the pipe that is read from
	handle_ptr stdout_;

	// an event that's given to pipe for overlapped reads, signalled when data
	// is available
	handle_ptr event_;

	// internal buffer of `buffer_size` bytes, the data from the pipe is put
	// in there
	std::unique_ptr<char[]> buffer_;

	// used for async reads
	OVERLAPPED ov_;

	// whether the last read attempt said an async operation was started
	bool pending_;

	// whether the last read attempt had `finished` true and nothing was
	// available in the pipe; in this case, the pipe is considered closed
	bool closed_;


	// creates the actual pipe, sets stdout_ and returns the other end so it
	// can be given to the process
	//
	HANDLE create_named_pipe();

	// called when pending_ is false; tries to read from the pipe, which may
	// start an async operation, in which case pending_ is set to true and an
	// empty string is returned
	//
	// if the read operation was completed synchronously, returns the bytes
	// that were read
	//
	std::string_view try_read();

	// called when pending_ is true; if the async operation is finished, resets
	// pending_ and returns the bytes that were read
	//
	std::string_view check_pending();
};


// a pipe connected to a process's stdin, it is written to; this pipe is
// synchronous and does not keep a copy of the given buffer, see write()
//
class async_pipe_stdin
{
public:
	async_pipe_stdin(const context& cx);

	handle_ptr create();

	// tries to send all of `s` down the pipe, returns the number of bytes
	// actually written
	//
	std::size_t write(std::string_view s);

	// closes the pipe, should be called as soon as everything has been written
	//
	void close();

private:
	const context& cx_;
	handle_ptr stdin_;
};

}	// namespace

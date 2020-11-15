#pragma once

#include "../utility.h"

namespace mob
{

class async_pipe_stdout
{
public:
	async_pipe_stdout(const context& cx);

	handle_ptr create();
	std::string_view read(bool finish);

	bool closed() const;

private:
	static const std::size_t buffer_size = 50'000;

	const context& cx_;
	handle_ptr stdout_;
	handle_ptr event_;
	std::unique_ptr<char[]> buffer_;
	OVERLAPPED ov_;
	bool pending_;
	bool closed_;

	HANDLE create_named_pipe();

	std::string_view try_read();
	std::string_view check_pending();
};


class async_pipe_stdin
{
public:
	async_pipe_stdin(const context& cx);

	handle_ptr create();
	std::size_t write(std::string_view s);

private:
	const context& cx_;
	handle_ptr stdin_;

	HANDLE create_anonymous_pipe();
};

}	// namespace

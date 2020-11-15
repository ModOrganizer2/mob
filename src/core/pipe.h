#pragma once

#include "../utility.h"

namespace mob
{

class async_pipe
{
public:
	async_pipe(const context& cx);

	handle_ptr create_for_stdout();
	std::string_view read(bool finish);

	handle_ptr create_for_stdin();
	std::size_t write(std::string_view s);

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

	handle_ptr create(bool for_stdout);
	HANDLE create_named_pipe();
	HANDLE create_anonymous_pipe();

	std::string_view try_read();
	std::string_view check_pending();
};

}	// namespace

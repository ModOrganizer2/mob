#pragma once

namespace mob
{

class context;

struct handle_closer
{
	using pointer = HANDLE;

	void operator()(HANDLE h)
	{
		if (h != INVALID_HANDLE_VALUE)
			::CloseHandle(h);
	}
};

using handle_ptr = std::unique_ptr<HANDLE, handle_closer>;


struct file_closer
{
	void operator()(std::FILE* f)
	{
		if (f)
			std::fclose(f);
	}
};

using file_ptr = std::unique_ptr<FILE, file_closer>;


class file_deleter
{
public:
	file_deleter(const context& cx, fs::path p);
	file_deleter(const file_deleter&) = delete;
	file_deleter& operator=(const file_deleter&) = delete;
	~file_deleter();

	void delete_now();
	void cancel();

private:
	const context& cx_;
	fs::path p_;
	bool delete_;
};


class directory_deleter
{
public:
	directory_deleter(const context& cx, fs::path p);
	directory_deleter(const directory_deleter&) = delete;
	directory_deleter& operator=(const directory_deleter&) = delete;
	~directory_deleter();

	void delete_now();
	void cancel();

private:
	const context& cx_;
	fs::path p_;
	bool delete_;
};


class interruption_file
{
public:
	interruption_file(const context& cx, fs::path dir, std::string name);

	fs::path file() const;
	bool exists() const;

	void create();
	void remove();

private:
	const context& cx_;
	fs::path dir_;
	std::string name_;
};


class bypass_file
{
public:
	bypass_file(const context& cx, fs::path dir, std::string name);

	bool exists() const;
	void create();

private:
	const context& cx_;
	fs::path file_;
};

} // namespace

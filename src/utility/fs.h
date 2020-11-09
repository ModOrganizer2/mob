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


// deletes the given file in the destructor unless cancel() is called
//
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


// deletes the given directory in the destructor unless cancel is called
//
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


// creates a temporary file in the given directory that is used to detect
// crashes or interruptions; some prefix is added to the filename to try and
// make it unlikely to clash
//
// for example:
//
//   interruption_file ifile("some/dir", "some action");
//
//   if (ifile.exists())
//   {
//      // action was previously interrupted, do something about it
//   }
//
//   // create interruption file
//   ifile.create();
//
//   // do stuff that might fail and throw or return early
//
//   // success, remove
//   ifile.remove();
//
class interruption_file
{
public:
	interruption_file(const context& cx, fs::path dir, std::string name);

	// path to the interruption file
	//
	fs::path file() const;

	// whether the file exists
	//
	bool exists() const;


	// creates the interruption file
	//
	void create();

	// removes the interruption file
	//
	void remove();

private:
	const context& cx_;
	fs::path dir_;
	std::string name_;
};


// creates a file in the given directory that is used to bypass an operation
// in the future; some prefix is added to the filename to try and make it
// unlikely to clash
//
// for example:
//
//   bypass_file built(cx, "some/dir/", "built");
//
//   if (built.exists())
//   {
//      // already built, bypass
//      return;
//   }
//
//   // do the build process
//
//   // bypass next time
//   built.create();
//
class bypass_file
{
public:
	bypass_file(const context& cx, fs::path dir, std::string name);

	// whether the bypass file exists
	//
	bool exists() const;

	// creates the bypass file
	//
	void create();

private:
	const context& cx_;
	fs::path file_;
};

} // namespace

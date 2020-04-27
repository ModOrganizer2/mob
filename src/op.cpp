#include "pch.h"
#include "op.h"
#include "utility.h"
#include "conf.h"

namespace builder
{

process::process()
	: code_(0)
{
}

process::process(std::string cmd, handle_ptr h)
	: cmd_(std::move(cmd)), handle_(std::move(h)), code_(0)
{
}

process::process(process&& p) :
	cmd_(std::move(p.cmd_)),
	handle_(std::move(p.handle_)),
	interrupt_(p.interrupt_.load()),
	code_(p.code_)
{
}

process& process::operator=(process&& p)
{
	cmd_ = std::move(p.cmd_);
	handle_ = std::move(p.handle_);
	interrupt_ = p.interrupt_.load();
	code_ = p.code_;
	return *this;
}

process::~process()
{
	join();
}

void process::interrupt()
{
	interrupt_ = true;
}

void process::join()
{
	if (!handle_)
		return;

	bool interrupted = false;

	for (;;)
	{
		const auto r = WaitForSingleObject(handle_.get(), 100);

		if (r == WAIT_OBJECT_0)
		{
			// done
			GetExitCodeProcess(handle_.get(), &code_);
			break;
		}

		if (r == WAIT_TIMEOUT)
		{
			if (interrupt_ && !interrupted)
			{
				const auto pid = GetProcessId(handle_.get());

				if (pid == 0)
				{
					error("process id is 0, terminating instead");
					::TerminateProcess(handle_.get(), 0xffff);
					break;
				}
				else
				{
					debug("sending sigint to " + std::to_string(pid));
					GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);
				}

				interrupted = true;
			}

			continue;
		}

		const auto e = GetLastError();
		handle_ = {};
		bail_out("failed to wait on process", e);
	}

	handle_ = {};
}

const std::string& process::cmd() const
{
	return cmd_;
}

int process::exit_code() const
{
	return static_cast<int>(code_);
}

void op::touch(const fs::path& p)
{
	debug("touching " + p.string());
	check(p);

	if (!conf::dry())
		do_touch(p);
}

void op::create_directories(const fs::path& p)
{
	debug("creating directory " + p.string());
	check(p);

	if (!conf::dry())
		do_create_directories(p);
}

void op::delete_directory(const fs::path& p)
{
	debug("deleting directory " + p.string());
	check(p);

	if (fs::exists(p) && !fs::is_directory(p))
		bail_out(p.string() + " is not a directory");

	if (!conf::dry())
		do_delete_directory(p);
}

void op::delete_file(const fs::path& p)
{
	if (!fs::exists(p))
		return;

	debug("deleting file " + p.string());
	check(p);

	if (fs::exists(p) && !fs::is_regular_file(p))
		bail_out("can't delete " + p.string() + ", not a file");

	if (!conf::dry())
		do_delete_file(p);
}

void op::remove_readonly(const fs::path& first)
{
	debug("removing read-only from " + first.string());
	check(first);

	if (!conf::dry())
	{
		if (fs::is_regular_file(first))
			do_remove_readonly(first);

		for (auto&& p : fs::recursive_directory_iterator(first))
		{
			if (fs::is_regular_file(p))
				do_remove_readonly(p);
		}
	}
}

bool is_source_better(const fs::path& src, const fs::path& dest)
{
	if (!fs::exists(dest))
	{
		debug("target " + dest.string() + " doesn't exist; copying");
		return true;
	}

	std::error_code ec;

	const auto src_size = fs::file_size(src, ec);
	if (ec)
	{
		warn("failed to get size of " + src.string() + "; forcing copy");
		return true;
	}

	const auto dest_size = fs::file_size(dest, ec);
	if (ec)
	{
		warn("failed to get size of " + dest.string() + "; forcing copy");
		return true;
	}

	if (src_size != dest_size)
	{
		debug(
			"src " + src.string() + " is " + std::to_string(src_size) + "), "
			"dest " + dest.string() + " is " + std::to_string(dest_size) + "); "
			"sizes different, copying");

		return true;
	}


	const auto src_time = fs::last_write_time(src, ec);
	if (ec)
	{
		warn("failed to get time of " + src.string() + "; forcing copy");
		return true;
	}

	const auto dest_time = fs::last_write_time(dest, ec);
	if (ec)
	{
		warn("failed to get time of " + dest.string() + "; forcing copy");
		return true;
	}

	if (src_time > dest_time)
	{
		debug(
			"src " + src.string() + " is newer than " + dest.string() + "; "
			"copying");

		return true;
	}

	// same size, same date
	return false;
}

void op::copy_file_to_dir_if_better(const fs::path& file, const fs::path& dir)
{
	check(file);
	check(dir);

	if (!conf::dry())
	{
		if (!fs::exists(file) || !fs::is_regular_file(file))
			bail_out("can't copy " + file.string() + ", not a file");

		if (fs::exists(dir) && !fs::is_directory(dir))
			bail_out("can't copy to " + dir.string() + ", not a directory");
	}

	const auto target = dir / file.filename();
	if (is_source_better(file, target))
	{
		info(file.string() + " -> " + dir.string());

		if (!conf::dry())
			do_copy_file_to_dir(file, dir);
	}
	else
	{
		debug("(skipped) " + file.string() + " -> " + dir.string());
	}
}

process op::run(const std::string& cmd, const fs::path& cwd)
{
	if (!cwd.empty())
		debug("> cd " + cwd.string());

	debug("> " + cmd);

	if (conf::dry())
		return {};

	return do_run(cmd, cwd);
}

void op::do_touch(const fs::path& p)
{
	std::ofstream out(p);
}

void op::do_create_directories(const fs::path& p)
{
	std::error_code ec;
	fs::create_directories(p, ec);

	if (ec)
		bail_out("can't create " + p.string(), ec);
}

void op::do_delete_directory(const fs::path& p)
{
	if (!fs::exists(p))
		return;

	std::error_code ec;
	fs::remove_all(p, ec);

	if (ec)
	{
		if (ec.value() == ERROR_ACCESS_DENIED)
		{
			remove_readonly(p);
			fs::remove_all(p, ec);

			if (!ec)
				return;
		}

		bail_out("failed to delete " + p.string(), ec);
	}
}

void op::do_delete_file(const fs::path& p)
{
	std::error_code ec;
	fs::remove(p, ec);

	if (ec)
		bail_out("can't delete " + p.string(), ec);
}

void op::do_copy_file_to_dir(const fs::path& f, const fs::path& d)
{
	create_directories(d);

	std::error_code ec;
	fs::copy_file(
		f, d / f.filename(),
		fs::copy_options::overwrite_existing, ec);

	if (ec)
		bail_out("can't copy " + f.string() + " to " + d.string(), ec);
}

void op::do_remove_readonly(const fs::path& p)
{
	std::error_code ec;
	fs::permissions(p, fs::perms::owner_write, fs::perm_options::add, ec);

	if (ec)
		bail_out("can't remove read-only flag on " + p.string(), ec);
}

process op::do_run(const std::string& what, const fs::path& cwd)
{
	STARTUPINFOA si = { .cb=sizeof(si) };
	PROCESS_INFORMATION pi = {};

	const std::string cmd = env("COMSPEC");
	const std::string args = "/C \"" + what + "\"";

	const char* cwd_p = nullptr;
	std::string cwd_s;

	if (!cwd.empty())
	{
		create_directories(cwd);
		cwd_s = cwd.string();
		cwd_p = (cwd_s.empty() ? nullptr : cwd_s.c_str());
	}

	const auto r = ::CreateProcessA(
		cmd.c_str(), const_cast<char*>(args.c_str()),
		nullptr, nullptr, FALSE, CREATE_NEW_PROCESS_GROUP,
		nullptr, cwd_p, &si, &pi);

	if (!r)
	{
		const auto e = GetLastError();
		bail_out("failed to start '" + cmd + "'", e);
	}

	::CloseHandle(pi.hThread);

	return process(what, handle_ptr(pi.hProcess));
}

void op::check(const fs::path& p)
{
	if (p.empty())
		bail_out("path is empty");

	if (p.native().starts_with(paths::prefix().native()))
		return;

	if (p.native().starts_with(paths::temp_dir().native()))
		return;

	bail_out("path " + p.string() + " is outside prefix");
}

}	// namespace

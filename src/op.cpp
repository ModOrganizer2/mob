#include "pch.h"
#include "op.h"
#include "utility.h"
#include "conf.h"

namespace builder
{

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
	: flags_(process::noflags), code_(0)
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

process process::raw(const std::string& cmd)
{
	process p;
	p.raw_ = cmd;
	return p;
}

process& process::name(const std::string& name)
{
	name_ = name;
	return *this;
}

const std::string& process::name() const
{
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

process& process::flags(flags_t f)
{
	flags_ = f;
	return *this;
}

process::flags_t process::flags() const
{
	return flags_;
}

process& process::env(const builder::env& e)
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

	std::string s = "\"" + bin_.string() + "\" " + cmd_.string();

	if ((flags_ & stdout_is_verbose) == 0)
		s += redir_nul();

	return s;
}

void process::pipe_into(const process& p)
{
	raw_ = make_cmd() + " | " + p.make_cmd();
}

void process::run()
{
	if (!cwd_.empty())
		debug("> cd " + cwd_.string());

	const auto what = make_cmd();
	debug("> " + what);

	if (conf::dry())
		return;

	do_run(what);
}

void process::do_run(const std::string& what)
{
	STARTUPINFOA si = { .cb=sizeof(si) };
	PROCESS_INFORMATION pi = {};

	const std::string cmd = current_env::get("COMSPEC");
	const std::string args = "/C \"" + what + "\"";

	const char* cwd_p = nullptr;
	std::string cwd_s;

	if (!cwd_.empty())
	{
		create_directories(cwd_);
		cwd_s = cwd_.string();
		cwd_p = (cwd_s.empty() ? nullptr : cwd_s.c_str());
	}

	const auto r = ::CreateProcessA(
		cmd.c_str(), const_cast<char*>(args.c_str()),
		nullptr, nullptr, FALSE, CREATE_NEW_PROCESS_GROUP,
		env_.get_pointers(), cwd_p, &si, &pi);

	if (!r)
	{
		const auto e = GetLastError();
		bail_out("failed to start '" + cmd + "'", e);
	}

	::CloseHandle(pi.hThread);
	impl_.handle.reset(pi.hProcess);
}

void process::interrupt()
{
	impl_.interrupt = true;
}

void process::join()
{
	if (!impl_.handle)
		return;

	bool interrupted = false;

	for (;;)
	{
		const auto r = WaitForSingleObject(impl_.handle.get(), 100);

		if (r == WAIT_OBJECT_0)
		{
			// done
			GetExitCodeProcess(impl_.handle.get(), &code_);

			if ((flags_ & allow_failure) || impl_.interrupt)
				break;

			if (code_ != 0)
			{
				impl_.handle = {};
				bail_out(make_name() + " returned " + std::to_string(code_));
			}

			break;
		}

		if (r == WAIT_TIMEOUT)
		{
			if (impl_.interrupt && !interrupted)
			{
				const auto pid = GetProcessId(impl_.handle.get());

				if (pid == 0)
				{
					error("process id is 0, terminating instead");
					::TerminateProcess(impl_.handle.get(), 0xffff);
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
		impl_.handle = {};
		bail_out("failed to wait on process", e);
	}

	impl_.handle = {};
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

void op::rename(const fs::path& src, const fs::path& dest)
{
	check(src);
	check(dest);

	if (fs::exists(dest))
	{
		bail_out(
			"can't rename " + src.string() + " to " + dest.string() + ", "
			"already exists");
	}

	debug("renaming " + src.string() + " to " + dest.string());
	do_rename(src, dest);
}

void op::move_to_directory(const fs::path& src, const fs::path& dest_dir)
{
	check(src);
	check(dest_dir);

	const auto target = dest_dir / src.filename();

	if (fs::exists(target))
	{
		bail_out(
			"can't move " + src.string() + " to " + dest_dir.string() + ", " +
			src.filename().string() + " already exists");
	}

	debug("moving " + src.string() + " to " + target.string());
	do_rename(src, target);
}

void op::copy_file_to_dir_if_better(
	const fs::path& file, const fs::path& dir, copy_flags f)
{
	check(file);
	check(dir);

	if (file.filename().string().find("*") == std::string::npos)
	{
		if (!conf::dry())
		{
			if (!fs::exists(file) || !fs::is_regular_file(file))
			{
				if (f & optional)
					return;

				bail_out("can't copy " + file.string() + ", not a file");
			}

			if (fs::exists(dir) && !fs::is_directory(dir))
				bail_out("can't copy to " + dir.string() + ", not a directory");
		}

		const auto target = dir / file.filename();
		if (is_source_better(file, target))
		{
			debug(file.string() + " -> " + dir.string());

			if (!conf::dry())
				do_copy_file_to_dir(file, dir);
		}
		else
		{
			debug("(skipped) " + file.string() + " -> " + dir.string());
		}
	}
	else
	{
		// wildcard
		const auto file_parent = file.parent_path();
		const auto wildcard = file.filename().string();

		for (auto&& e : fs::directory_iterator(file_parent))
		{
			const auto name = e.path().filename().string();

			if (PathMatchSpecA(name.c_str(), wildcard.c_str()))
				copy_file_to_dir_if_better(e.path(), dir);
		}
	}
}

void op::do_touch(const fs::path& p)
{
	create_directories(p.parent_path());

	std::ofstream out(p);
	if (!out)
		bail_out("failed to touch " + p.string());
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

void op::do_rename(const fs::path& src, const fs::path& dest)
{
	std::error_code ec;
	fs::rename(src, dest, ec);

	if (ec)
		bail_out("can't rename " + src.string() + " to " + dest.string(), ec);
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

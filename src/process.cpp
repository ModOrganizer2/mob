#include "pch.h"
#include "process.h"
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

}	// namespace

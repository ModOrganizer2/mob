#include "utility.h"

namespace builder
{

class process
{
public:
	enum flags_t
	{
		noflags = 0x00,
		stdout_is_verbose = 0x01,
		allow_failure = 0x02
	};

	process();
	~process();

	static process raw(const std::string& cmd);

	static process pipe(const process& p)
	{
		return p;
	}

	template <class... Processes>
	static process pipe(const process& p1, const process& p2, Processes&&... ps)
	{
		auto r = p1;
		r.pipe_into(p2);
		pipe(r, std::forward<Processes>(ps)...);
		return r;
	}

	process& name(const std::string& name);
	const std::string& name() const;

	process& binary(const fs::path& p);
	const fs::path& binary() const;

	process& cwd(const fs::path& p);
	const fs::path& cwd() const;

	process& flags(flags_t f);
	flags_t flags() const;

	template <class... Args>
	process& arg(Args&&... args)
	{
		cmd_.arg(std::forward<Args>(args)...);
		return *this;
	}

	template <template<class, class> class Container, class K, class V, class Alloc>
	process& args(const Container<std::pair<K, V>, Alloc>& v, cmd::flags f=cmd::noflags)
	{
		for (auto&& [name, value] : v)
			arg(name, value, f);

		return *this;
	}

	template <class Container>
	process& args(const Container& v, cmd::flags f=cmd::noflags)
	{
		for (auto&& e : v)
			cmd_.arg(e, f);

		return *this;
	}

	process& env(const builder::env& e);

	void run();
	void interrupt();
	void join();

	int exit_code() const;

private:
	struct impl
	{
		handle_ptr handle;
		std::atomic<bool> interrupt{false};

		impl() = default;
		impl(const impl&);
		impl& operator=(const impl&);
	};

	std::string name_;
	fs::path bin_;
	fs::path cwd_;
	flags_t flags_;
	cmd cmd_;
	builder::env env_;
	std::string raw_;

	impl impl_;
	DWORD code_;

	std::string make_name() const;
	std::string make_cmd() const;
	void pipe_into(const process& p);
	void do_run(const std::string& what);
};

}	// namespace

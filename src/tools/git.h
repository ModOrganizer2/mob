#pragma once

namespace mob
{

class git_wrap
{
public:
	static fs::path binary();

	git_wrap(fs::path root, basic_process_runner* runner=nullptr);

	void clone(const mob::url& url, const std::string& branch, bool shallow);
	void pull(const mob::url& url, const std::string& branch);

	void set_credentials(const std::string& username, const std::string& email);

	void set_remote(
		std::string org, std::string key,
		bool no_push_upstream, bool push_default_origin);

	void ignore_ts(bool b);
	void revert_ts();
	bool is_tracked(const fs::path& file);

	bool has_remote(const std::string& name);

	void add_remote(
		const std::string& remote_name,
		const std::string& org, const std::string& key, bool push_default,
		const std::string& url_pattern={}, const std::string& git_file={});

	void rename_remote(const std::string& from, const std::string& to);

	void set_remote_push(const std::string& remote, const std::string& url);

	void set_config(const std::string& key, const std::string& value);

	void set_assume_unchanged(const fs::path& relative_file, bool on);

	std::string git_file();

	void init_repo();

	void apply(const std::string& diff);

	void fetch(const std::string& remote, const std::string& branch);

	void checkout(const std::string& what);

	void add_submodule(
		const std::string& branch, const std::string& submodule,
		const mob::url& url);

	std::string current_branch();

	bool is_git_repo();

	bool has_uncommitted_changes();
	bool has_stashed_changes();

	static void delete_directory(const context& cx, const fs::path& dir);
	static bool remote_branch_exists(const mob::url& u, const std::string& name);

private:
	fs::path root_;
	basic_process_runner* runner_;

	int run(process&& p);
	int run(process& p);

	const context& cx();

	static std::string make_url(
		const std::string& org, const std::string& git_file,
		const std::string& url_pattern={});
};



class git : public basic_process_runner
{
public:
	enum ops
	{
		clone = 1,
		pull,
		clone_or_pull
	};

	git(ops o);

	git& url(const mob::url& u);
	git& root(const fs::path& dir);
	git& branch(const std::string& name);

	git& ignore_ts_on_clone(bool b);
	git& revert_ts_on_pull(bool b);
	git& credentials(const std::string& username, const std::string& email);
	git& shallow(bool b);

	git& remote(
		std::string org, std::string key,
		bool no_push_upstream, bool push_default_origin);

protected:
	void do_run() override;

private:
	ops op_;
	mob::url url_;
	fs::path root_;
	std::string branch_;

	bool ignore_ts_ = false;
	bool revert_ts_ = false;
	std::string creds_username_;
	std::string creds_email_;
	bool shallow_ = false;
	std::string remote_org_;
	std::string remote_key_;
	bool no_push_upstream_ = false;
	bool push_default_origin_ = false;

	void do_clone_or_pull();
	bool do_clone();
	void do_pull();
};


class git_submodule : public basic_process_runner
{
public:
	git_submodule();

	git_submodule& url(const mob::url& u);
	git_submodule& root(const fs::path& dir);
	git_submodule& branch(const std::string& name);
	git_submodule& submodule(const std::string& name);

	const std::string& submodule() const;

protected:
	void do_run() override;

private:
	mob::url url_;
	fs::path root_;
	std::string branch_;
	std::string submodule_;
};


class git_submodule_adder : public instrumentable<2>
{
public:
	enum class times
	{
		add_submodule_wait,
		add_submodule
	};

	~git_submodule_adder();

	static git_submodule_adder& instance();

	void queue(git_submodule g);
	void stop();

private:
	struct sleeper
	{
		std::mutex m;
		std::condition_variable cv;
		bool ready = false;
	};

	context cx_;
	std::thread thread_;
	std::vector<git_submodule> queue_;
	mutable std::mutex queue_mutex_;
	std::atomic<bool> quit_;
	sleeper sleeper_;

	git_submodule_adder();
	void run();

	void thread_fun();
	void wakeup();
	void process();
};

}	// namespace

#pragma once

namespace mob
{

class git : public basic_process_runner
{
public:
	enum ops
	{
		clone = 1,
		pull,
		clone_or_pull,
		add_submodule
	};

	struct creds
	{
		std::string username;
		std::string email;
	};


	git(ops o);

	static void delete_directory(const context& cx, const fs::path& p);

	static fs::path binary();

	static void set_credentials(
		const fs::path& repo,
		const std::string& username, const std::string& email);

	static void set_remote(
		const fs::path& repo,
		std::string org, std::string key,
		bool no_push_upstream, bool push_default_origin);

	static void ignore_ts(const fs::path& repo, bool b);

	static bool has_remote(const fs::path& repo, const std::string& name);

	static void add_remote(
		const fs::path& repo, const std::string& remote_name,
		const std::string& org, const std::string& key,
		bool push_default, const std::string& url_pattern={});

	static bool is_git_repo(const fs::path& p);
	static bool remote_branch_exists(const mob::url& u, const std::string& name);
	static void init_repo(const fs::path& p);

	static void apply(const fs::path& p, const std::string& diff);
	static void fetch(
		const fs::path& p,
		const std::string& remote, const std::string& branch);
	static void checkout(const fs::path& p, const std::string& what);

	static std::string current_branch(const fs::path& p);


	git& url(const mob::url& u);
	git& branch(const std::string& name);
	git& root(const fs::path& dir);
	const fs::path& root() const;
	git& credentials(const std::string& username, const std::string& email);
	git& submodule_name(const std::string& name);
	const std::string& submodule_name() const;
	git& ignore_ts_on_clone(bool b);
	git& revert_ts_on_pull(bool b);
	git& shallow(bool b);
	bool is_tracked(const fs::path& relative_file);

	git& remote(
		std::string org, std::string key,
		bool no_push_upstream, bool push_default_origin);

protected:
	void do_run() override;

private:
	ops op_;
	mob::url url_;
	std::string branch_;
	fs::path root_;
	std::string submodule_;
	std::string creds_username_;
	std::string creds_email_;
	std::string remote_org_;
	std::string remote_key_;
	bool no_push_upstream_ = false;
	bool push_default_origin_ = false;
	bool ignore_ts_ = false;
	bool revert_ts_ = false;
	bool shallow_ = false;

	void do_clone_or_pull();
	bool do_clone();
	void do_pull();
	void do_add_submodule();

	void do_set_credentials();
	void do_set_remote();
	void do_ignore_ts();
	void do_revert_ts();

	void set_config(const std::string& key, const std::string& value);
	bool has_remote(const std::string& name);
	void rename_remote(const std::string& from, const std::string& to);
	void add_remote(const std::string& name, const std::string& url);
	void set_remote_push(const std::string& remote, const std::string& url);
	void set_assume_unchanged(const fs::path& relative_file, bool on);
	void checkout(const std::string& what);
	bool is_repo();
	bool remote_branch_exists();
	bool has_uncommitted_changes();
	bool has_stashed_changes();
	void init();

	std::string git_file();

	static std::string make_url(
		const std::string& org, const std::string& git_file,
		const std::string& url_pattern={});
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

	void queue(git g);
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
	std::vector<git> queue_;
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

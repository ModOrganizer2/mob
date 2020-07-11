#pragma once

namespace mob
{

class task;

class command
{
public:
	struct common_options
	{
		bool dry = false;
		int output_log_level = -1;
		int file_log_level = -1;
		std::string log_file;
		std::vector<std::string> options;
		std::vector<std::string> inis;
		bool no_default_inis = false;
		bool dump_inis = false;
		std::string prefix;
	};

	struct meta_t
	{
		std::string name, description;
	};

	static common_options common;
	static clipp::group common_options_group();

	virtual ~command() = default;

	void force_exit_code(int code);
	void force_pick();
	void force_help();

	bool picked() const;
	bool wants_help() const;

	clipp::group group();
	int run();

	const std::vector<fs::path>& inis() const;

	virtual meta_t meta() const = 0;

protected:
	enum flags
	{
		noflags           = 0x00,
		requires_options  = 0x01,
	};

	bool picked_;
	bool help_;

	command(flags f=noflags);

	virtual void convert_cl_to_conf();

	void set_task_enabled_flags(const std::vector<std::string>& tasks);
	int load_options();
	int prepare_options(bool verbose);

	virtual clipp::group do_group() = 0;
	virtual int do_run() = 0;
	virtual std::string do_doc() { return {}; }

private:
	flags flags_;
	std::optional<int> code_;
	std::vector<fs::path> inis_;

	int gather_inis(bool verbose);
};


class version_command : public command
{
public:
	meta_t meta() const override;

protected:
	clipp::group do_group() override;
	int do_run() override;
};


class help_command : public command
{
public:
	meta_t meta() const override;
	void set_commands(const std::vector<std::shared_ptr<command>>& v);

protected:
	clipp::group do_group() override;
	int do_run() override;

private:
	std::string commands_;
};


class options_command : public command
{
public:
	options_command();
	meta_t meta() const override;

protected:
	clipp::group do_group() override;
	int do_run() override;
	std::string do_doc() override;
};


class build_command : public command
{
public:
	build_command();
	meta_t meta() const override;

protected:
	void convert_cl_to_conf() override;
	clipp::group do_group() override;
	int do_run() override;

private:
	std::vector<std::string> tasks_;
	bool redownload_ = false;
	bool reextract_ = false;
	bool rebuild_ = false;
	bool reconfigure_ = false;
	bool new_ = false;
	std::optional<bool> clean_;
	std::optional<bool> fetch_;
	std::optional<bool> build_;
	std::optional<bool> nopull_;
	bool ignore_uncommitted_ = false;
	bool keep_msbuild_ = false;
	std::optional<bool> revert_ts_;

	void terminate_msbuild();
};


class list_command : public command
{
public:
	meta_t meta() const override;

protected:
	clipp::group do_group() override;
	int do_run() override;
	std::string do_doc() override;

private:
	bool all_ = false;
	std::vector<std::string> tasks_;

	void dump(const std::vector<task*>& v, std::size_t indent) const;
};


class release_command : public command
{
public:
	release_command();
	meta_t meta() const override;

	void make_bin();
	void make_pdbs();
	void make_src();

protected:
	clipp::group do_group() override;
	int do_run() override;
	std::string do_doc() override;

private:
	bool bin_ = true;
	bool src_ = true;
	bool pdbs_ = true;
	std::string utf8out_;
	fs::path out_;
	std::string version_;
	bool version_exe_ = false;
	bool version_rc_ = false;
	std::string utf8_rc_path_;
	fs::path rc_path_;
	bool force_;
	std::string suffix_;

	fs::path make_filename(const std::string& what) const;

	void walk_dir(
		const fs::path& dir, std::vector<fs::path>& files,
		const std::vector<std::regex>& ignore_re, std::size_t& total_size);

	std::string version_from_exe() const;
	std::string version_from_rc() const;
};


class git_command : public command
{
public:
	git_command();
	meta_t meta() const override;

protected:
	clipp::group do_group() override;
	int do_run() override;
	std::string do_doc() override;

private:
	enum class modes
	{
		none = 0,
		set_remotes,
		add_remote,
		ignore_ts
	};

	modes mode_ = modes::none;
	std::string username_;
	std::string email_;
	std::string key_;
	std::string remote_;
	std::string path_;
	bool tson_ = false;
	bool nopush_ = false;
	bool push_default_ = false;

	void do_set_remotes();
	void do_set_remotes(const fs::path& r);

	void do_add_remote();
	void do_add_remote(const fs::path& r);

	void do_ignore_ts();
	void do_ignore_ts(const fs::path& r);

	std::vector<fs::path> get_repos() const;
};


class cmake_command : public command
{
public:
	cmake_command();
	meta_t meta() const override;

protected:
	clipp::group do_group() override;
	int do_run() override;
	std::string do_doc() override;

private:
	std::string gen_;
	std::string cmd_;
	bool x64_ = true;
	std::string prefix_;
	std::string path_;
};


class inis_command : public command
{
public:
	meta_t meta() const override;

protected:
	clipp::group do_group() override;
	int do_run() override;
	std::string do_doc() override;
};


class tx_command : public command
{
public:
	tx_command();
	meta_t meta() const override;

protected:
	clipp::group do_group() override;
	int do_run() override;
	std::string do_doc() override;

private:
	enum class modes
	{
		none = 0,
		get
	};

	modes mode_ = modes::none;
	int min_ = -1;
	std::string key_;
	std::string url_;
	bool force_ = false;
	std::string path_;

	void do_get();
};

}	// namespace

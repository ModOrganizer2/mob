#pragma once

namespace mob
{

class command
{
public:
	virtual ~command() = default;

	struct common_options
	{
		bool dry = false;
		int output_log_level = -1;
		int file_log_level = -1;
		std::string log_file;
		std::vector<std::string> options;
		std::vector<std::string> set;
		std::string ini;
		std::string prefix;
	};

	static common_options common;

	static clipp::group common_options_group();

	void force_exit_code(int code);
	void force_pick();

	bool picked() const;
	bool wants_help() const;

	clipp::group group();
	int run();

protected:
	enum flags
	{
		noflags           = 0x00,
		requires_options  = 0x01,
	};

	bool picked_;
	bool help_;

	command(flags f=noflags);

	virtual clipp::group do_group() = 0;
	virtual void do_pre_run() {};
	virtual int do_run() = 0;
	virtual std::string do_doc() { return {}; }

private:
	flags flags_;
	std::optional<int> code_;
};


class version_command : public command
{
public:

protected:
	clipp::group do_group() override;
	int do_run() override;
};


class help_command : public command
{
public:

protected:
	clipp::group do_group() override;
	int do_run() override;
};


class options_command : public command
{
public:

protected:
	clipp::group do_group() override;
	int do_run() override;
	std::string do_doc() override;
};


class build_command : public command
{
public:
	build_command();

protected:
	clipp::group do_group() override;
	void do_pre_run() override;
	int do_run() override;

private:
	std::vector<std::string> tasks_;
	bool redownload_ = false;
	bool reextract_ = false;
	bool rebuild_ = false;
	bool clean_ = false;
	bool keep_msbuild_ = false;

	void terminate_msbuild();
};


class list_command : public command
{
public:

protected:
	clipp::group do_group() override;
	int do_run() override;
	std::string do_doc() override;
};


class release_command : public command
{
public:
	release_command();

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

}	// namespace

#pragma once

namespace mob
{

class command
{
public:
	command();
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
	virtual clipp::group group() = 0;

	void force_exit_code(int code);
	void force_pick();

	bool picked() const;

	int run();

protected:
	bool picked_;

	virtual int do_run() = 0;

private:
	std::optional<int> code_;
};


class version_command : public command
{
public:
	clipp::group group() override;

protected:
	int do_run() override;
};


class help_command : public command
{
public:
	clipp::group group() override;

protected:
	int do_run() override;
};


class options_command : public command
{
public:
	clipp::group group() override;

protected:
	int do_run() override;
};


class build_command : public command
{
public:
	clipp::group group() override;

protected:
	int do_run() override;

private:
	std::vector<std::string> tasks_;
	bool redownload_ = false;
	bool reextract_ = false;
	bool rebuild_ = false;
	bool clean_ = false;
};


class list_command : public command
{
public:
	clipp::group group() override;

protected:
	int do_run() override;
};


class devbuild_command : public command
{
public:
	clipp::group group() override;

protected:
	int do_run() override;
};

}	// namespace

#pragma once

namespace mob
{

class msbuild : public basic_process_runner
{
public:
	enum flags_t
	{
		noflags        = 0x00,
		single_job     = 0x01,
		allow_failure  = 0x02
	};

	enum ops
	{
		build = 1,
		clean
	};

	msbuild(ops o=build);

	static fs::path binary();

	msbuild& solution(const fs::path& sln);
	msbuild& targets(const std::vector<std::string>& names);
	msbuild& parameters(const std::vector<std::string>& params);
	msbuild& config(const std::string& s);
	msbuild& platform(const std::string& s);
	msbuild& architecture(arch a);
	msbuild& flags(flags_t f);
	msbuild& prepend_path(const fs::path& p);

	int result() const;

protected:
	void do_run() override;

private:
	ops op_;
	fs::path sln_;
	std::vector<std::string> targets_;
	std::vector<std::string> params_;
	std::string config_;
	std::string platform_;
	arch arch_;
	flags_t flags_;
	std::vector<fs::path> prepend_path_;

	void do_clean();
	void do_build();
	void do_run(const std::vector<std::string>& targets);
};

MOB_ENUM_OPERATORS(msbuild::flags_t);

}	// namespace

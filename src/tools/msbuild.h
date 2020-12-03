#pragma once

namespace mob
{

// tool that runs `msbuild`, see the jom tool for explanations on errors with
// parallel builds and the single_job/allow_failure flags, the same thing
// happens with msbuild
//
class msbuild : public basic_process_runner
{
public:
	// path to the msbuild binary
	//
	static fs::path binary();


	enum flags_t
	{
		noflags        = 0x00,
		single_job     = 0x01,
		allow_failure  = 0x02
	};

	// what run() should do
	//
	enum ops
	{
		build = 1,
		clean
	};

	msbuild(ops o=build);

	// .sln file
	//
	msbuild& solution(const fs::path& sln);

	// adds a "-target:string` for each string given
	//
	msbuild& targets(const std::vector<std::string>& names);

	// adds a "-property:string" for every string given
	//
	msbuild& properties(const std::vector<std::string>& props);

	// sets "-property:Configuration=s"
	//
	msbuild& config(const std::string& s);

	// sets "-property:Platform=s"; if not set, uses architecture() to figure
	// it out
	//
	msbuild& platform(const std::string& s);

	// used by
	//  1) the vsvars environment variables
	//  2) the "-property:Platform" property if platform() wasn't called
	//
	msbuild& architecture(arch a);

	// flags
	msbuild& flags(flags_t f);

	// can be called multiple times, the given paths will be prepended to PATH
	// before invoking msbuild
	//
	msbuild& prepend_path(const fs::path& p);

	// exit code
	//
	int result() const;

protected:
	void do_run() override;

private:
	ops op_;
	fs::path sln_;
	std::vector<std::string> targets_;
	std::vector<std::string> props_;
	std::string config_;
	std::string platform_;
	arch arch_;
	flags_t flags_;
	std::vector<fs::path> prepend_path_;

	// runs msbuild with ":Clean" for each target given in targets(), giving
	// something like "-target:modorganizer:Clean"
	//
	void do_clean();

	// runs msbuild
	//
	void do_build();

	// called by both do_clean() and do_build
	//
	void run_for_targets(const std::vector<std::string>& targets);

	std::string platform_property() const;
};

MOB_ENUM_OPERATORS(msbuild::flags_t);

}	// namespace

#include "pch.h"
#include "tools.h"
#include "../conf.h"

namespace mob
{

msbuild::msbuild() :
	basic_process_runner("msbuild"), config_("Release"), arch_(arch::def),
	flags_(noflags)
{
}

fs::path msbuild::binary()
{
	return conf::tool_by_name("msbuild");
}

msbuild& msbuild::solution(const fs::path& sln)
{
	sln_ = sln;
	return *this;
}

msbuild& msbuild::projects(const std::vector<std::string>& names)
{
	projects_ = names;
	return *this;
}

msbuild& msbuild::parameters(const std::vector<std::string>& params)
{
	params_ = params;
	return *this;
}

msbuild& msbuild::config(const std::string& s)
{
	config_ = s;
	return *this;
}

msbuild& msbuild::platform(const std::string& s)
{
	platform_ = s;
	return *this;
}

msbuild& msbuild::architecture(arch a)
{
	arch_ = a;
	return *this;
}

msbuild& msbuild::flags(flags_t f)
{
	flags_ = f;
	return *this;
}

int msbuild::result() const
{
	return exit_code();
}

void msbuild::do_run()
{
	// 14.2 to v142
	const auto toolset = "v" + replace_all(vs::toolset(), ".", "");

	std::string plat;

	if (platform_.empty())
	{
		switch (arch_)
		{
			case arch::x86:
				plat = "Win32";
				break;

			case arch::x64:
				plat = "x64";
				break;

			case arch::dont_care:
			default:
				bail_out("msbuild::do_run(): bad arch");
		}
	}
	else
	{
		plat = platform_;
	}

	process::flags_t pflags = process::noflags;
	if (flags_ & allow_failure)
	{
		process_.stderr_level(context::level::trace);
		pflags |= process::allow_failure;
	}

	process_
		.binary(binary())
		.chcp(65001)
		.stdout_encoding(encodings::utf8)
		.stderr_encoding(encodings::utf8)
		.arg("-nologo");

	if ((flags_ & single_job) == 0)
	{
		process_
			.arg("-maxCpuCount")
			.arg("-property:UseMultiToolTask=true")
			.arg("-property:EnforceProcessCountAcrossBuilds=true");
	}

	process_
		.arg("-property:Configuration=", config_, process::quote)
		.arg("-property:PlatformToolset=" + toolset)
		.arg("-property:WindowsTargetPlatformVersion=" + vs::sdk())
		.arg("-property:Platform=", plat, process::quote)
		.arg("-verbosity:minimal", process::log_quiet)
		.arg("-consoleLoggerParameters:ErrorsOnly", process::log_quiet);

	if (!projects_.empty())
		process_.arg("-target:" + mob::join(projects_, ","));

	for (auto&& p : params_)
		process_.arg("-property:" + p);

	process_
		.arg(sln_)
		.flags(pflags)
		.cwd(sln_.parent_path())
		.env(env::vs(arch_));

	execute_and_join();
}

}	// namespace

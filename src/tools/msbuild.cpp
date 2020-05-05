#include "pch.h"
#include "tools.h"
#include "../conf.h"

namespace mob
{

msbuild::msbuild()
	: basic_process_runner("msbuild"), config_("Release"), arch_(arch::def)
{
	process_
		.binary(tool_paths::msbuild());
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

void msbuild::do_run()
{
	// 14.2 to v142
	const auto toolset = "v" + replace_all(versions::vs_toolset(), ".", "");

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


	process_
		.arg("-nologo")
		.arg("-maxCpuCount")
		.arg("-property:UseMultiToolTask=true")
		.arg("-property:EnforceProcessCountAcrossBuilds=true")
		.arg("-property:Configuration=", config_, process::quote)
		.arg("-property:PlatformToolset=" + toolset)
		.arg("-property:WindowsTargetPlatformVersion=" + versions::sdk())
		.arg("-property:Platform=", plat, process::quote)
		.arg("-verbosity:minimal", process::log_quiet)
		.arg("-consoleLoggerParameters:ErrorsOnly", process::log_quiet);

	if (!projects_.empty())
		process_.arg("-target:" + mob::join(projects_, ","));

	for (auto&& p : params_)
		process_.arg("-property:" + p);

	process_
		.arg(sln_)
		.cwd(sln_.parent_path())
		.env(env::vs(arch_));

	execute_and_join();
}

}	// namespace

#include "pch.h"
#include "tools.h"
#include "../conf.h"

namespace mob
{

msbuild::msbuild(ops o) :
	basic_process_runner("msbuild"),
	op_(o), config_("Release"), arch_(arch::def), flags_(noflags)
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

msbuild& msbuild::targets(const std::vector<std::string>& names)
{
	targets_ = names;
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

msbuild& msbuild::prepend_path(const fs::path& p)
{
	prepend_path_.push_back(p);
	return *this;
}

int msbuild::result() const
{
	return exit_code();
}

void msbuild::do_run()
{
	switch (op_)
	{
		case clean:
		{
			do_clean();
			break;
		}

		case build:
		{
			do_build();
			break;
		}

		default:
		{
			cx().bail_out(context::generic, "bad msbuild op {}", op_);
		}
	}
}

void msbuild::do_build()
{
	do_run(targets_);
}

void msbuild::do_run(const std::vector<std::string>& targets)
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

	if (is_set(flags_, allow_failure))
	{
		process_.stderr_level(context::level::trace);
		pflags |= process::allow_failure;
	}
	else
	{
		process_.stdout_filter([&](auto& f){ error_filter(f); });
	}

	process_
		.binary(binary())
		.chcp(65001)
		.stdout_encoding(encodings::utf8)
		.stderr_encoding(encodings::utf8)
		.arg("-nologo");

	if (!is_set(flags_, single_job))
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
		.arg("-property:RunCodeAnalysis=false")
		.arg("-verbosity:minimal", process::log_quiet)
		.arg("-consoleLoggerParameters:ErrorsOnly", process::log_quiet);

	if (!targets.empty())
		process_.arg("-target:" + mob::join(targets, ";"));

	for (auto&& p : params_)
		process_.arg("-property:" + p);

	env e = env::vs(arch_);

	for (auto&& p : prepend_path_)
		e.prepend_path(p);

	process_
		.arg(sln_)
		.flags(pflags)
		.cwd(sln_.parent_path())
		.env(e);

	execute_and_join();
}

void msbuild::do_clean()
{
	flags_ |= allow_failure;
	do_run(map(targets_, [&](auto&& t){ return t + ":Clean"; }));
}

void msbuild::error_filter(process::filter& f) const
{
	// ": error C2065"
	// ": error MSB1009"
	static std::regex re(": error [A-Z]");
	if (std::regex_search(f.line.begin(), f.line.end(), re))
		f.lv = context::level::error;
}

}	// namespace

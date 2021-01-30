#include "pch.h"
#include "tools.h"
#include "../core/conf.h"
#include "../core/process.h"
#include "../core/env.h"

namespace mob
{

msbuild::msbuild(ops o) :
	basic_process_runner("msbuild"),
	op_(o), config_("Release"), arch_(arch::def), flags_(noflags)
{
}

fs::path msbuild::binary()
{
	return conf().tool().get("msbuild");
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

msbuild& msbuild::properties(const std::vector<std::string>& props)
{
	props_ = props;
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

msbuild& msbuild::env(const mob::env& e)
{
	env_ = e;
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
	run_for_targets(targets_);
}

std::string msbuild::platform_property() const
{
	if (!platform_.empty())
		return platform_;

	switch (arch_)
	{
		case arch::x86:
			return "Win32";

		case arch::x64:
			return "x64";

		case arch::dont_care:
		default:
			cx().bail_out(context::generic, "msbuild::do_run(): bad arch");
	}
}

void msbuild::run_for_targets(const std::vector<std::string>& targets)
{
	// 14.2 to v142
	const auto toolset = "v" + replace_all(vs::toolset(), ".", "");

	process p;

	if (is_set(flags_, allow_failure))
	{
		// make sure errors are not displayed and mob doesn't bail out
		p
			.stderr_level(context::level::trace)
			.flags(process::allow_failure);
	}
	else
	{
		p.stdout_filter([&](auto& f)
		{
			// ": error C2065"
			// ": error MSB1009"
			static std::regex re(": error [A-Z]");

			// ghetto attempt at showing errors on the console, since stdout
			// has all the compiler output
			if (std::regex_search(f.line.begin(), f.line.end(), re))
				f.lv = context::level::error;
		});
	}

	// msbuild will use the console's encoding, so by invoking `chcp 65001`
	// (the utf8 "codepage"), stdout and stderr are utf8
	p
		.binary(binary())
		.chcp(65001)
		.stdout_encoding(encodings::utf8)
		.stderr_encoding(encodings::utf8)
		.arg("-nologo");

	if (!is_set(flags_, single_job))
	{
		// multi-process
		p
			.arg("-maxCpuCount")
			.arg("-property:UseMultiToolTask=true")
			.arg("-property:EnforceProcessCountAcrossBuilds=true");
	}

	p
		.arg("-property:Configuration=", config_, process::quote)
		.arg("-property:PlatformToolset=" + toolset)
		.arg("-property:WindowsTargetPlatformVersion=" + vs::sdk())
		.arg("-property:Platform=", platform_property(), process::quote)
		.arg("-verbosity:minimal", process::log_quiet)
		.arg("-consoleLoggerParameters:ErrorsOnly", process::log_quiet);

	// some projects have code analysis turned on and can fail on preview
	// versions, make sure it's never run
	p.arg("-property:RunCodeAnalysis=false");

	// targets
	if (!targets.empty())
		p.arg("-target:" + mob::join(targets, ";"));

	// properties
	for (auto&& prop : props_)
		p.arg("-property:" + prop);

	p
		.arg(sln_)
		.cwd(sln_.parent_path())
		.env(env_ ? *env_ : env::vs(arch_));

	execute_and_join(p);
}

void msbuild::do_clean()
{
	flags_ |= allow_failure;
	run_for_targets(map(targets_, [&](auto&& t){ return t + ":Clean"; }));
}

}	// namespace

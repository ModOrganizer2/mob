#include "pch.h"
#include "tools.h"
#include "utility.h"
#include "op.h"
#include "conf.h"

namespace builder
{

fs::path find_vcvars()
{
	const std::vector<std::string> editions =
	{
		"Preview", "Enterprise", "Professional", "Community"
	};

	for (auto&& edition : editions)
	{
		const auto p =
			paths::program_files_x86() /
			"Microsoft Visual Studio" /
			versions::vs_year() /
			edition / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat";

		if (fs::exists(p))
		{
			debug("found " + p.string());
			return p;
		}
	}

	bail_out("couldn't find visual studio");
}

void vcvars()
{
	const fs::path tmp = paths::temp_file();

	const std::string cmd =
		"\"" + find_vcvars().string() + "\" amd64 " + redir_nul() + " && "
		"set > " + tmp.string();

	op::run(cmd);

	std::stringstream ss(read_text_file(tmp));
	op::delete_file(tmp);

	for (;;)
	{
		std::string line;
		std::getline(ss, line);
		if (!ss)
			break;

		const auto sep = line.find('=');

		if (sep == std::string::npos)
			continue;

		const std::string name = line.substr(0, sep);
		const std::string value = line.substr(sep + 1);

		debug("setting env " + name);
		SetEnvironmentVariableA(name.c_str(), nullptr);
		SetEnvironmentVariableA(name.c_str(), value.c_str());
	}
}


void expand(const fs::path& file, const fs::path& where)
{
	if (fs::exists(where))
	{
		debug(
			"not expanding " + file.string() + ", " +
			where.string() + " already exists");

		return;
	}

	const auto sevenz = find_sevenz();

	op::delete_directory(where);
	op::create_directories(where);

	if (file.string().ends_with(".tar.gz"))
	{
		op::run(
			sevenz.string() + " x -so \"" + file.string() + "\" | " +
			sevenz.string() + " x -spe -si -ttar -o\"" + where.string() + "\" " + redir_nul());
	}
	else
	{
		op::run(
			sevenz.string() + " x -spe -bd -bb0 -o\"" + where.string() + "\" "
			"\"" + file.string() + "\" " + redir_nul());
	}
}


void cmake(const fs::path& build, const fs::path& prefix, const std::string& generator)
{
	std::string cmd = "cmake -G \"" + generator + "\"";

	if (!prefix.empty())
		cmd += " -DCMAKE_INSTALL_PREFIX=\"" + prefix.string() + "\"";

	cmd += " ..",
		op::run(cmd, build);
}

fs::path cmake_for_nmake(const fs::path& root, const fs::path& prefix)
{
	const auto build = root / "build";
	const std::string g = "NMake Makefiles";
	cmake(build, prefix, g);
	return build;
}

fs::path cmake_for_vs(const fs::path& root, const fs::path& prefix)
{
	const auto build = root / "vsbuild";
	const std::string g = "Visual Studio " + versions::vs_version() + " " + versions::vs_year();
	cmake(build, prefix, g);
	return build;
}

void nmake(const fs::path& dir)
{
	op::run("nmake", dir);
	op::run("nmake install", dir);
}

}	// namespace

#include "pch.h"
#include "tools.h"
#include "../conf.h"

namespace mob
{

patcher::patcher()
	: basic_process_runner("patcher")
{
}

patcher& patcher::task(const std::string& name)
{
	patches_ = paths::patches() / name;
	return *this;
}

patcher& patcher::file(const fs::path& p)
{
	file_ = p;
	return *this;
}

patcher& patcher::root(const fs::path& dir)
{
	output_ = dir;
	return *this;
}

void patcher::do_run()
{
	if (!fs::exists(patches_))
		return;

	if (file_.empty())
	{
		for (auto e : fs::directory_iterator(patches_))
		{
			if (!e.is_regular_file())
				continue;

			const auto p = e.path();

			if (p.extension() == ".manual_patch")
			{
				// skip manual patches
				continue;
			}
			else if (p.extension() != ".patch")
			{
				warn(
					"file without .patch extension " + p.string() + " "
					"in patches directory " + patches_.string());

				continue;
			}

			do_patch(p);
		}
	}
	else
	{
		do_patch(patches_ / file_);
	}
}

void patcher::do_patch(const fs::path& patch_file)
{
	const auto base = process()
		.binary(third_party::patch())
		.arg("--read-only", "ignore")
		.arg("--strip", "0")
		.arg("--directory", output_)
		.arg("--quiet", process::quiet);

	const auto check = process(base)
		.flags(process::allow_failure|process::stdout_is_verbose)
		.arg("--dry-run")
		.arg("--force")
		.arg("--reverse")
		.arg("--input", patch_file);

	const auto apply = process(base)
		.arg("--forward")
		.arg("--batch")
		.arg("--input", patch_file);

	{
		// check
		process_ = check;
		if (execute_and_join() == 0)
		{
			debug("patch " + patch_file.string() + " already applied");
			return;
		}
	}

	{
		// apply
		process_ = apply;
		debug("applying patch " + patch_file.string());
		execute_and_join();
	}
}

}	// namespace

#include "pch.h"
#include "tools.h"
#include "../conf.h"

namespace mob
{

patcher::patcher()
	: basic_process_runner("patch")
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
	{
		cx_->trace(context::generic,
			"patch directory " + patches_.string() + " doesn't exist, "
			"assuming no patches");

		return;
	}

	if (file_.empty())
	{
		cx_->trace(context::generic,
			"looking for patches in " + patches_.string());

		for (auto e : fs::directory_iterator(patches_))
		{
			if (!e.is_regular_file())
			{
				cx_->trace(context::generic,
					"skipping " + e.path().string() + ", not a file");

				continue;
			}

			const auto p = e.path();

			if (p.extension() == ".manual_patch")
			{
				cx_->trace(context::generic,
					"skipping manual patch " + e.path().string());

				continue;
			}
			else if (p.extension() != ".patch")
			{
				cx_->warning(context::generic,
					"file with unknown extension " + p.string());

				continue;
			}

			do_patch(p);
		}
	}
	else
	{
		cx_->trace(context::generic,
			"doing manual patch from " + file_.string());

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
		.arg("--quiet", process::log_quiet);

	const auto check = process(base)
		.flags(process::allow_failure)
		.arg("--dry-run")
		.arg("--force")
		.arg("--reverse")
		.arg("--input", patch_file);

	const auto apply = process(base)
		.arg("--forward")
		.arg("--batch")
		.arg("--input", patch_file);

	cx_->trace(context::generic,
		"trying to patch using " + patch_file.string());

	{
		// check

		cx_->trace(context::generic,
			"checking if already patched");

		process_ = check;
		const auto ret = execute_and_join();

		if (ret == 0)
		{
			cx_->trace(context::generic,
				"patch " + patch_file.string() + " already applied");

			return;
		}
		else if (ret == 1)
		{
			cx_->trace(context::generic,
				"looks like the patch is needed");
		}
		else
		{
			cx_->bail_out(context::generic,
				"patch returned " + std::to_string(ret));
		}
	}

	{
		// apply

		cx_->trace(context::generic, "applying patch " + patch_file.string());
		process_ = apply;
		execute_and_join();
	}
}

}	// namespace

#include "pch.h"
#include "tools.h"
#include "../core/conf.h"
#include "../core/process.h"

namespace mob
{

patcher::patcher()
	: basic_process_runner("patch"), prebuilt_(false)
{
}

fs::path patcher::binary()
{
	return conf().tool().get("patch");
}

patcher& patcher::task(const std::string& name, bool prebuilt)
{
	task_ = name;
	prebuilt_ = prebuilt;
	return *this;
}

patcher& patcher::file(const fs::path& p)
{
	file_ = p;
	return *this;
}

patcher& patcher::root(const fs::path& dir)
{
	root_ = dir;
	return *this;
}

void patcher::do_run()
{
	// patches should be in mob/patches/task-name, but not all tasks need
	// patches
	const fs::path root = conf().path().patches() / task_;

	if (!fs::exists(root))
	{
		cx().trace(context::generic,
			"patch directory {} doesn't exist, assuming no patches", root);

		return;
	}


	if (!file_.empty())
	{
		// patcher tool is being run for a manual patch
		cx().trace(context::generic, "doing manual patch from {}", file_);
		do_patch(root / file_);
		return;
	}

	// patcher tool is being by the task for auto patching, figure out the
	// directory to use depending on whether it's a prebuilt
	const fs::path patches = root / (prebuilt_ ? "prebuilt" : "sources");
	cx().trace(context::generic, "looking for patches in {}", patches);

	if (!fs::exists(patches))
	{
		cx().trace(context::generic,
			"patch directory {} doesn't exist, assuming no patches",
			patches);

		return;
	}


	// for each path file
	for (auto e : fs::directory_iterator(patches))
	{
		if (!e.is_regular_file())
		{
			cx().trace(context::generic, "skipping {}, not a file", e.path());
			continue;
		}

		const auto p = e.path();

		if (p.extension() == ".manual_patch")
		{
			cx().trace(context::generic, "skipping manual patch {}", e.path());
			continue;
		}
		else if (p.extension() != ".patch")
		{
			cx().warning(context::generic, "file with unknown extension {}", p);
			continue;
		}

		do_patch(p);
	}
}

void patcher::do_patch(const fs::path& patch)
{
	// there's no way to figure out if patch failure is because 1) the patch
	// file is incorrect, or 2) the patch has already been applied
	//
	// an incorrect patch file would probably mean that the source has changed
	// and the patch must be updated or removed if it's not required anymore
	//
	// so patching is a two step process: check if the patch has already been
	// applied, and apply it if it hasn't

	// use by both the check and apply processes
	const auto base = process()
		.binary(binary())
		.arg("--read-only", "ignore")
		.arg("--strip", "0")
		.arg("--directory", root_)
		.arg("--quiet", process::log_quiet);

	// process to reverse the path: the only way to check if a patch has been
	// applied is actually to try to reverse it and check if there was an error
	//
	// this uses --dry-run because if the file was already patched, it shouldn't
	// actually be reversed
	auto check = process(base)
		.flags(process::allow_failure)
		.arg("--dry-run")
		.arg("--force")    // no prompts
		.arg("--reverse")  // swaps old and new files
		.arg("--input", patch);

	// process to apply the patch
	auto apply = process(base)
		.arg("--forward")  // don't try to reverse the patch if it fails
		.arg("--batch")    // no prompts
		.arg("--input", patch);

	cx().trace(context::generic, "trying to patch using {}", patch);

	{
		// check, returns 0 when the patch would have been reversed correctly,
		// 1 if not, anything else on error
		cx().trace(context::generic, "checking if already patched");
		const auto ret = execute_and_join(check);

		if (ret == 0)
		{
			// reversing the patch would succeed, so the patch has already been
			// applied
			cx().trace(context::generic, "patch {} already applied", patch);
			return;
		}

		// anything other than 0 or 1 is a hard error
		if (ret != 1)
			cx().bail_out(context::generic, "patch returned {}", ret);

		cx().trace(context::generic, "looks like the patch is needed");
	}

	{
		// apply
		cx().trace(context::generic, "applying patch {}", patch);
		execute_and_join(apply);
	}
}

}	// namespace

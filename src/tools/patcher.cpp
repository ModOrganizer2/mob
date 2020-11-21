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
	output_ = dir;
	return *this;
}

void patcher::do_run()
{
	const fs::path patches_root = conf().path().patches() / task_;

	if (!fs::exists(patches_root))
	{
		cx().trace(context::generic,
			"patch directory {} doesn't exist, assuming no patches",
			patches_root);

		return;
	}

	if (file_.empty())
	{
		const fs::path patches =
			patches_root / (prebuilt_ ? "prebuilt" : "sources");

		cx().trace(context::generic, "looking for patches in {}", patches);

		if (!fs::exists(patches))
		{
			cx().trace(context::generic,
				"patch directory {} doesn't exist, assuming no patches",
				patches);

			return;
		}

		for (auto e : fs::directory_iterator(patches))
		{
			if (!e.is_regular_file())
			{
				cx().trace(context::generic,
					"skipping {}, not a file", e.path());

				continue;
			}

			const auto p = e.path();

			if (p.extension() == ".manual_patch")
			{
				cx().trace(context::generic,
					"skipping manual patch {}", e.path());

				continue;
			}
			else if (p.extension() != ".patch")
			{
				cx().warning(context::generic,
					"file with unknown extension {}", p);

				continue;
			}

			do_patch(p);
		}
	}
	else
	{
		cx().trace(context::generic, "doing manual patch from {}", file_);
		do_patch(patches_root / file_);
	}
}

void patcher::do_patch(const fs::path& patch_file)
{
	const auto base = process()
		.binary(binary())
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

	cx().trace(context::generic, "trying to patch using {}", patch_file);

	{
		// check

		cx().trace(context::generic,
			"checking if already patched");

		set_process(check);
		const auto ret = execute_and_join();

		if (ret == 0)
		{
			cx().trace(context::generic,
				"patch {} already applied", patch_file);

			return;
		}
		else if (ret == 1)
		{
			cx().trace(context::generic,
				"looks like the patch is needed");
		}
		else
		{
			cx().bail_out(context::generic, "patch returned {}", ret);
		}
	}

	{
		// apply

		cx().trace(context::generic, "applying patch {}", patch_file);
		set_process(apply);
		execute_and_join();
	}
}

}	// namespace

#include "pch.h"
#include "op.h"
#include "utility.h"
#include "conf.h"

namespace mob
{

void op::touch(const fs::path& p)
{
	debug("touching " + p.string());
	check(p);

	if (!conf::dry())
		do_touch(p);
}

void op::create_directories(const fs::path& p)
{
	debug("creating directory " + p.string());
	check(p);

	if (!conf::dry())
		do_create_directories(p);
}

void op::delete_directory(const fs::path& p)
{
	debug("deleting directory " + p.string());
	check(p);

	if (fs::exists(p) && !fs::is_directory(p))
		bail_out(p.string() + " is not a directory");

	if (!conf::dry())
		do_delete_directory(p);
}

void op::delete_file(const fs::path& p)
{
	if (!fs::exists(p))
		return;

	debug("deleting file " + p.string());
	check(p);

	if (fs::exists(p) && !fs::is_regular_file(p))
		bail_out("can't delete " + p.string() + ", not a file");

	if (!conf::dry())
		do_delete_file(p);
}

void op::remove_readonly(const fs::path& first)
{
	debug("removing read-only from " + first.string());
	check(first);

	if (!conf::dry())
	{
		if (fs::is_regular_file(first))
			do_remove_readonly(first);

		for (auto&& p : fs::recursive_directory_iterator(first))
		{
			if (fs::is_regular_file(p))
				do_remove_readonly(p);
		}
	}
}

bool is_source_better(const fs::path& src, const fs::path& dest)
{
	if (!fs::exists(dest))
	{
		debug("target " + dest.string() + " doesn't exist; copying");
		return true;
	}

	std::error_code ec;

	const auto src_size = fs::file_size(src, ec);
	if (ec)
	{
		warn("failed to get size of " + src.string() + "; forcing copy");
		return true;
	}

	const auto dest_size = fs::file_size(dest, ec);
	if (ec)
	{
		warn("failed to get size of " + dest.string() + "; forcing copy");
		return true;
	}

	if (src_size != dest_size)
	{
		debug(
			"src " + src.string() + " is " + std::to_string(src_size) + "), "
			"dest " + dest.string() + " is " + std::to_string(dest_size) + "); "
			"sizes different, copying");

		return true;
	}


	const auto src_time = fs::last_write_time(src, ec);
	if (ec)
	{
		warn("failed to get time of " + src.string() + "; forcing copy");
		return true;
	}

	const auto dest_time = fs::last_write_time(dest, ec);
	if (ec)
	{
		warn("failed to get time of " + dest.string() + "; forcing copy");
		return true;
	}

	if (src_time > dest_time)
	{
		debug(
			"src " + src.string() + " is newer than " + dest.string() + "; "
			"copying");

		return true;
	}

	// same size, same date
	return false;
}

void op::rename(const fs::path& src, const fs::path& dest)
{
	check(src);
	check(dest);

	if (fs::exists(dest))
	{
		bail_out(
			"can't rename " + src.string() + " to " + dest.string() + ", "
			"already exists");
	}

	debug("renaming " + src.string() + " to " + dest.string());
	do_rename(src, dest);
}

void op::move_to_directory(const fs::path& src, const fs::path& dest_dir)
{
	check(src);
	check(dest_dir);

	const auto target = dest_dir / src.filename();

	if (fs::exists(target))
	{
		bail_out(
			"can't move " + src.string() + " to " + dest_dir.string() + ", " +
			src.filename().string() + " already exists");
	}

	debug("moving " + src.string() + " to " + target.string());
	do_rename(src, target);
}

void op::copy_file_to_dir_if_better(
	const fs::path& file, const fs::path& dir, copy_flags f)
{
	check(file);
	check(dir);

	if (file.filename().string().find("*") == std::string::npos)
	{
		if (!conf::dry())
		{
			if (!fs::exists(file) || !fs::is_regular_file(file))
			{
				if (f & optional)
					return;

				bail_out("can't copy " + file.string() + ", not a file");
			}

			if (fs::exists(dir) && !fs::is_directory(dir))
				bail_out("can't copy to " + dir.string() + ", not a directory");
		}

		const auto target = dir / file.filename();
		if (is_source_better(file, target))
		{
			debug(file.string() + " -> " + dir.string());

			if (!conf::dry())
				do_copy_file_to_dir(file, dir);
		}
		else
		{
			debug("(skipped) " + file.string() + " -> " + dir.string());
		}
	}
	else
	{
		// wildcard
		const auto file_parent = file.parent_path();
		const auto wildcard = file.filename().string();

		for (auto&& e : fs::directory_iterator(file_parent))
		{
			const auto name = e.path().filename().string();

			if (PathMatchSpecA(name.c_str(), wildcard.c_str()))
				copy_file_to_dir_if_better(e.path(), dir);
		}
	}
}

void op::do_touch(const fs::path& p)
{
	create_directories(p.parent_path());

	std::ofstream out(p);
	if (!out)
		bail_out("failed to touch " + p.string());
}

void op::do_create_directories(const fs::path& p)
{
	std::error_code ec;
	fs::create_directories(p, ec);

	if (ec)
		bail_out("can't create " + p.string(), ec);
}

void op::do_delete_directory(const fs::path& p)
{
	if (!fs::exists(p))
		return;

	std::error_code ec;
	fs::remove_all(p, ec);

	if (ec)
	{
		if (ec.value() == ERROR_ACCESS_DENIED)
		{
			remove_readonly(p);
			fs::remove_all(p, ec);

			if (!ec)
				return;
		}

		bail_out("failed to delete " + p.string(), ec);
	}
}

void op::do_delete_file(const fs::path& p)
{
	std::error_code ec;
	fs::remove(p, ec);

	if (ec)
		bail_out("can't delete " + p.string(), ec);
}

void op::do_copy_file_to_dir(const fs::path& f, const fs::path& d)
{
	create_directories(d);

	std::error_code ec;
	fs::copy_file(
		f, d / f.filename(),
		fs::copy_options::overwrite_existing, ec);

	if (ec)
		bail_out("can't copy " + f.string() + " to " + d.string(), ec);
}

void op::do_remove_readonly(const fs::path& p)
{
	std::error_code ec;
	fs::permissions(p, fs::perms::owner_write, fs::perm_options::add, ec);

	if (ec)
		bail_out("can't remove read-only flag on " + p.string(), ec);
}

void op::do_rename(const fs::path& src, const fs::path& dest)
{
	std::error_code ec;
	fs::rename(src, dest, ec);

	if (ec)
		bail_out("can't rename " + src.string() + " to " + dest.string(), ec);
}

void op::check(const fs::path& p)
{
	if (p.empty())
		bail_out("path is empty");

	if (p.native().starts_with(paths::prefix().native()))
		return;

	if (p.native().starts_with(paths::temp_dir().native()))
		return;

	bail_out("path " + p.string() + " is outside prefix");
}

}	// namespace

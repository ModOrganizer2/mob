#include "pch.h"
#include "op.h"
#include "utility.h"
#include "conf.h"
#include "context.h"

namespace mob::op
{

void do_touch(const fs::path& p, const context* cx);
void do_create_directories(const fs::path& p, const context* cx);
void do_delete_directory(const fs::path& p, const context* cx);
void do_delete_file(const fs::path& p, const context* cx);
void do_copy_file_to_dir(const fs::path& f, const fs::path& d, const context* cx);
void do_remove_readonly(const fs::path& p, const context* cx);
void do_rename(const fs::path& src, const fs::path& dest, const context* cx);
void check(const fs::path& p, const context* cx);


void touch(const fs::path& p, const context* cx)
{
	if (!cx)
		cx = context::global();

	cx->log(context::op, "touching " + p.string());
	check(p, cx);

	if (!conf::dry())
		do_touch(p, cx);
}

void create_directories(const fs::path& p, const context* cx)
{
	if (!cx)
		cx = context::global();

	cx->log(context::op, "creating dir " + p.string());
	check(p, cx);

	if (!conf::dry())
		do_create_directories(p, cx);
}

void delete_directory(const fs::path& p, flags f, const context* cx)
{
	if (!cx)
		cx = context::global();

	cx->log(context::op, "deleting dir " + p.string());
	check(p, cx);

	if (!fs::exists(p))
	{
		if (f & optional)
		{
			cx->log(
				context::op_trace,
				"not deleting dir " + p.string() + ", doesn't exist");

			return;
		}

		cx->bail_out("can't delete dir " + p.string() + ", doesn't exist");
	}

	if (fs::exists(p) && !fs::is_directory(p))
		cx->bail_out(p.string() + " is not a dir");

	if (!conf::dry())
		do_delete_directory(p, cx);
}

void delete_file(const fs::path& p, flags f, const context* cx)
{
	if (!cx)
		cx = context::global();

	cx->log(context::op, "deleting file " + p.string());
	check(p, cx);

	if (!fs::exists(p))
	{
		if (f & optional)
		{
			cx->log(
				context::op_trace,
				"not deleting file " + p.string() + ", doesn't exist");

			return;
		}

		cx->bail_out("can't delete file " + p.string() + ", doesn't exist");
	}

	if (fs::exists(p) && !fs::is_regular_file(p))
		cx->bail_out("can't delete " + p.string() + ", not a file");

	if (!conf::dry())
		do_delete_file(p, cx);
}

void remove_readonly(const fs::path& first, const context* cx)
{
	if (!cx)
		cx = context::global();

	cx->log(context::op, "removing read-only from " + first.string());
	check(first, cx);

	if (!conf::dry())
	{
		if (fs::is_regular_file(first))
			do_remove_readonly(first, cx);

		for (auto&& p : fs::recursive_directory_iterator(first))
		{
			if (fs::is_regular_file(p))
				do_remove_readonly(p, cx);
		}
	}
}

bool is_source_better(
	const fs::path& src, const fs::path& dest, const context* cx)
{
	if (!fs::exists(dest))
	{
		cx->log(
			context::op,
			"target " + dest.string() + " doesn't exist; copying");

		return true;
	}

	std::error_code ec;

	const auto src_size = fs::file_size(src, ec);
	if (ec)
	{
		cx->log(
			context::warning,
			"failed to get size of " + src.string() + "; forcing copy");

		return true;
	}

	const auto dest_size = fs::file_size(dest, ec);
	if (ec)
	{
		cx->log(
			context::warning,
			"failed to get size of " + dest.string() + "; forcing copy");

		return true;
	}

	if (src_size != dest_size)
	{
		cx->log(
			context::op,
			"src " + src.string() + " is " + std::to_string(src_size) + "), "
			"dest " + dest.string() + " is " + std::to_string(dest_size) + "); "
			"sizes different, copying");

		return true;
	}


	const auto src_time = fs::last_write_time(src, ec);
	if (ec)
	{
		cx->log(
			context::warning,
			"failed to get time of " + src.string() + "; forcing copy");

		return true;
	}

	const auto dest_time = fs::last_write_time(dest, ec);
	if (ec)
	{
		cx->log(
			context::warning,
			"failed to get time of " + dest.string() + "; forcing copy");

		return true;
	}

	if (src_time > dest_time)
	{
		cx->log(
			context::op,
			"src " + src.string() + " is newer than " + dest.string() + "; "
			"copying");

		return true;
	}

	// same size, same date
	return false;
}

void rename(const fs::path& src, const fs::path& dest, const context* cx)
{
	if (!cx)
		cx = context::global();

	check(src, cx);
	check(dest, cx);

	if (fs::exists(dest))
	{
		cx->bail_out(
			"can't rename " + src.string() + " to " + dest.string() + ", "
			"already exists");
	}

	cx->log(
		context::op,
		"renaming " + src.string() + " to " + dest.string());

	do_rename(src, dest, cx);
}

void move_to_directory(
	const fs::path& src, const fs::path& dest_dir, const context* cx)
{
	check(src, cx);
	check(dest_dir, cx);

	const auto target = dest_dir / src.filename();

	if (fs::exists(target))
	{
		cx->bail_out(
			"can't move " + src.string() + " to " + dest_dir.string() + ", " +
			src.filename().string() + " already exists");
	}

	cx->log(context::op, "moving " + src.string() + " to " + target.string());
	do_rename(src, target, cx);
}

void copy_file_to_dir_if_better(
	const fs::path& file, const fs::path& dir, flags f, const context* cx)
{
	check(file, cx);
	check(dir, cx);

	if (file.filename().string().find("*") == std::string::npos)
	{
		if (!conf::dry())
		{
			if (!fs::exists(file) || !fs::is_regular_file(file))
			{
				if (f & optional)
				{
					cx->log(
						context::op,
						"not copying " + file.string() + ", not found but "
						"optional");

					return;
				}

				cx->bail_out("can't copy " + file.string() + ", not a file");
			}

			if (fs::exists(dir) && !fs::is_directory(dir))
				cx->bail_out("can't copy to " + dir.string() + ", not a dir");
		}

		const auto target = dir / file.filename();
		if (is_source_better(file, target, cx))
		{
			cx->log(context::op, file.string() + " -> " + dir.string());

			if (!conf::dry())
				do_copy_file_to_dir(file, dir, cx);
		}
		else
		{
			cx->log(
				context::bypass,
				"(skipped) " + file.string() + " -> " + dir.string());
		}
	}
	else
	{
		cx->log(context::op_trace, file.filename().string() + " is a glob");

		// wildcard
		const auto file_parent = file.parent_path();
		const auto wildcard = file.filename().string();

		for (auto&& e : fs::directory_iterator(file_parent))
		{
			const auto name = e.path().filename().string();

			if (PathMatchSpecA(name.c_str(), wildcard.c_str()))
				copy_file_to_dir_if_better(e.path(), dir, op::noflags, cx);
			else
				cx->log(context::op_trace, name + " did not match " + wildcard);
		}
	}
}

void do_touch(const fs::path& p, const context* cx)
{
	op::create_directories(p.parent_path(), cx);

	std::ofstream out(p);
	if (!out)
		cx->bail_out("failed to touch " + p.string());
}

void do_create_directories(const fs::path& p, const context* cx)
{
	std::error_code ec;
	fs::create_directories(p, ec);

	if (ec)
		cx->bail_out("can't create " + p.string(), ec);
}

void do_delete_directory(const fs::path& p, const context* cx)
{
	std::error_code ec;
	fs::remove_all(p, ec);

	if (ec)
	{
		if (ec.value() == ERROR_ACCESS_DENIED)
		{
			cx->log(
				context::op,
				"got access denied trying to delete dir " + p.string() + ", "
				"trying to remove read-only flag recursively");

			remove_readonly(p, cx);
			fs::remove_all(p, ec);

			if (!ec)
				return;
		}

		cx->bail_out("failed to delete " + p.string(), ec);
	}
}

void do_delete_file(const fs::path& p, const context* cx)
{
	std::error_code ec;
	fs::remove(p, ec);

	if (ec)
		cx->bail_out("can't delete " + p.string(), ec);
}

void do_copy_file_to_dir(
	const fs::path& f, const fs::path& d, const context* cx)
{
	op::create_directories(d, cx);

	std::error_code ec;
	fs::copy_file(
		f, d / f.filename(),
		fs::copy_options::overwrite_existing, ec);

	if (ec)
		cx->bail_out("can't copy " + f.string() + " to " + d.string(), ec);
}

void do_remove_readonly(const fs::path& p, const context* cx)
{
	cx->log(context::op_trace, "chmod +x " + p.string());

	std::error_code ec;
	fs::permissions(p, fs::perms::owner_write, fs::perm_options::add, ec);

	if (ec)
		cx->bail_out("can't remove read-only flag on " + p.string(), ec);
}

void do_rename(const fs::path& src, const fs::path& dest, const context* cx)
{
	std::error_code ec;
	fs::rename(src, dest, ec);

	if (ec)
	{
		cx->bail_out(
			"can't rename " + src.string() + " to " + dest.string(), ec);
	}
}

void check(const fs::path& p, const context* cx)
{
	if (p.empty())
		cx->bail_out("path is empty");

	if (p.native().starts_with(paths::prefix().native()))
		return;

	if (p.native().starts_with(paths::temp_dir().native()))
		return;

	cx->bail_out("path " + p.string() + " is outside prefix");
}

}	// namespace

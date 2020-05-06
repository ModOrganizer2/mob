#include "pch.h"
#include "op.h"
#include "utility.h"
#include "conf.h"
#include "context.h"

namespace mob::op
{

void do_touch(const context& cx, const fs::path& p);
void do_create_directories(const context& cx, const fs::path& p);
void do_delete_directory(const context& cx, const fs::path& p);
void do_delete_file(const context& cx, const fs::path& p);
void do_copy_file_to_dir(const context& cx, const fs::path& f, const fs::path& d);
void do_remove_readonly(const context& cx, const fs::path& p);
void do_rename(const context& cx, const fs::path& src, const fs::path& dest);
void check(const context& cx, const fs::path& p);


void touch(const context& cx, const fs::path& p)
{
	cx.trace(context::fs, "touching " + p.string());
	check(cx, p);

	if (!conf::dry())
		do_touch(cx ,p);
}

void create_directories(const context& cx, const fs::path& p)
{
	cx.trace(context::fs, "creating dir " + p.string());
	check(cx, p);

	if (!conf::dry())
		do_create_directories(cx, p);
}

void delete_directory(const context& cx, const fs::path& p, flags f)
{
	cx.trace(context::fs, "deleting dir " + p.string());
	check(cx, p);

	if (!fs::exists(p))
	{
		if (f & optional)
		{
			cx.trace(context::fs,
				"not deleting dir " + p.string() + ", "
				"doesn't exist (optional)");

			return;
		}

		cx.bail_out(context::fs,
			"can't delete dir " + p.string() + ", doesn't exist");
	}

	if (fs::exists(p) && !fs::is_directory(p))
		cx.bail_out(context::fs, p.string() + " is not a dir");

	if (!conf::dry())
		do_delete_directory(cx, p);
}

void delete_file(const context& cx, const fs::path& p, flags f)
{
	cx.trace(context::fs, "deleting file " + p.string());
	check(cx, p);

	if (!fs::exists(p))
	{
		if (f & optional)
		{
			cx.trace(context::fs,
				"not deleting file " + p.string() + ", "
				"doesn't exist (optional)");

			return;
		}

		cx.bail_out(context::fs,
			"can't delete file " + p.string() + ", doesn't exist");
	}

	if (fs::exists(p) && !fs::is_regular_file(p))
	{
		cx.bail_out(context::fs,
			"can't delete " + p.string() + ", not a file");
	}

	if (!conf::dry())
		do_delete_file(cx, p);
}

void remove_readonly(const context& cx, const fs::path& first)
{
	cx.trace(context::fs, "removing read-only from " + first.string());
	check(cx, first);

	if (!conf::dry())
	{
		if (fs::is_regular_file(first))
			do_remove_readonly(cx, first);

		for (auto&& p : fs::recursive_directory_iterator(first))
		{
			if (fs::is_regular_file(p))
				do_remove_readonly(cx, p);
		}
	}
}

bool is_source_better(
	const context& cx, const fs::path& src, const fs::path& dest)
{
	if (!fs::exists(dest))
	{
		cx.trace(context::fs,
			"target " + dest.string() + " doesn't exist; copying");

		return true;
	}

	std::error_code ec;

	const auto src_size = fs::file_size(src, ec);
	if (ec)
	{
		cx.warning(context::fs,
			"failed to get size of " + src.string() + "; forcing copy");

		return true;
	}

	const auto dest_size = fs::file_size(dest, ec);
	if (ec)
	{
		cx.warning(context::fs,
			"failed to get size of " + dest.string() + "; forcing copy");

		return true;
	}

	if (src_size != dest_size)
	{
		cx.trace(
			context::fs,
			"src " + src.string() + " is " + std::to_string(src_size) + "), "
			"dest " + dest.string() + " is " + std::to_string(dest_size) + "); "
			"sizes different, copying");

		return true;
	}


	const auto src_time = fs::last_write_time(src, ec);
	if (ec)
	{
		cx.warning(context::fs,
			"failed to get time of " + src.string() + "; forcing copy");

		return true;
	}

	const auto dest_time = fs::last_write_time(dest, ec);
	if (ec)
	{
		cx.warning(context::fs,
			"failed to get time of " + dest.string() + "; forcing copy");

		return true;
	}

	if (src_time > dest_time)
	{
		cx.trace(context::fs,
			"src " + src.string() + " is newer than " + dest.string() + "; "
			"copying");

		return true;
	}

	// same size, same date
	return false;
}

void rename(const context& cx, const fs::path& src, const fs::path& dest)
{
	check(cx, src);
	check(cx, dest);

	if (fs::exists(dest))
	{
		cx.bail_out(context::fs,
			"can't rename " + src.string() + " to " + dest.string() + ", "
			"already exists");
	}

	cx.trace(context::fs, "renaming " + src.string() + " to " + dest.string());
	do_rename(cx, src, dest);
}

void move_to_directory(
	const context& cx, const fs::path& src, const fs::path& dest_dir)
{
	check(cx, src);
	check(cx, dest_dir);

	const auto target = dest_dir / src.filename();

	if (fs::exists(target))
	{
		cx.bail_out(context::fs,
			"can't move " + src.string() + " to " + dest_dir.string() + ", " +
			src.filename().string() + " already exists");
	}

	cx.trace(context::fs, "moving " + src.string() + " to " + target.string());
	do_rename(cx, src, target);
}

void copy_file_to_dir_if_better(
	const context& cx, const fs::path& file, const fs::path& dir, flags f)
{
	if ((f & unsafe) == 0)
	{
		check(cx, file);
		check(cx, dir);
	}

	if (file.string().find("*") != std::string::npos)
		cx.bail_out(context::fs, file.string() + " contains a glob");

	if (!conf::dry())
	{
		if (!fs::exists(file) || !fs::is_regular_file(file))
		{
			if (f & optional)
			{
				cx.trace(context::fs,
					"not copying " + file.string() + ", "
					"doesn't exist (optional)");

				return;
			}

			cx.bail_out(context::fs,
				"can't copy " + file.string() + ", not a file");
		}

		if (fs::exists(dir) && !fs::is_directory(dir))
		{
			cx.bail_out(context::fs,
				"can't copy to " + dir.string() + ", not a dir");
		}
	}

	const auto target = dir / file.filename();
	if (is_source_better(cx, file, target))
	{
		cx.trace(context::fs, file.string() + " -> " + dir.string());

		if (!conf::dry())
			do_copy_file_to_dir(cx, file, dir);
	}
	else
	{
		cx.trace(context::bypass,
			"(skipped) " + file.string() + " -> " + dir.string());
	}
}

void copy_glob_to_dir_if_better(
	const context& cx,
	const fs::path& src_glob, const fs::path& dest_dir, flags f)
{
	const auto file_parent = src_glob.parent_path();
	const auto wildcard = src_glob.filename().string();

	for (auto&& e : fs::directory_iterator(file_parent))
	{
		const auto name = e.path().filename().string();

		if (!PathMatchSpecA(name.c_str(), wildcard.c_str()))
		{
			cx.trace(context::fs,
				name + " did not match " + wildcard + "; skipping");

			continue;
		}

		if (e.is_regular_file())
		{
			if (f & copy_files)
			{
				copy_file_to_dir_if_better(cx, e.path(), dest_dir);
			}
			else
			{
				cx.trace(context::fs,
					"file " + name + " matched " + wildcard + " "
					"but files are not copied");
			}
		}
		else if (e.is_directory())
		{
			if (f & copy_dirs)
			{
				const fs::path sub = dest_dir / e.path().filename();

				create_directories(cx, sub);
				copy_glob_to_dir_if_better(cx, e.path() / "*", sub, f);
			}
			else
			{
				cx.trace(context::fs,
					"directory " + name + " matched " + wildcard + " "
					"but directories are not copied");
			}
		}
	}
}

std::string read_text_file(const context& cx, const fs::path& p, flags f)
{
	cx.trace(context::fs, "reading " + p.string());

	std::string s;
	std::ifstream in(p);

	in.seekg(0, std::ios::end);
	s.resize(static_cast<std::size_t>(in.tellg()));
	in.seekg(0, std::ios::beg);
	in.read(&s[0], static_cast<std::streamsize>(s.size()));

	if (in.bad())
	{
		if (f & optional)
		{
			cx.debug(context::fs,
				"can't read from " + p.string() + " (optional)");
		}
		else
		{
			cx.bail_out(context::fs, "can't read from " + p.string());
		}
	}
	else
	{
		cx.trace(context::fs,
			"finished reading " + p.string() + ", " +
			std::to_string(s.size()) + " bytes");
	}

	return s;
}

void write_text_file(
	const context& cx, const fs::path& p, std::string_view s, flags f)
{
	check(cx, p);

	cx.trace(context::fs, "writing " + p.string());

	{
		std::ofstream out(p);
		out << s;
		out.close();

		if (out.bad())
		{
			if (f & optional)
			{
				cx.debug(context::fs,
					"can't write to " + p.string() + " (optional)");
			}
			else
			{
				cx.bail_out(context::fs, "can't write to " + p.string());
			}
		}
	}

	cx.trace(context::fs,
		"finished writing " + p.string() + ", " +
		std::to_string(s.size()) + " bytes");
}


void do_touch(const context& cx, const fs::path& p)
{
	op::create_directories(cx, p.parent_path());

	std::ofstream out(p);
	if (!out)
		cx.bail_out(context::fs, "failed to touch " + p.string());
}

void do_create_directories(const context& cx, const fs::path& p)
{
	std::error_code ec;
	fs::create_directories(p, ec);

	if (ec)
		cx.bail_out(context::fs, "can't create " + p.string(), ec);
}

void do_delete_directory(const context& cx, const fs::path& p)
{
	std::error_code ec;
	fs::remove_all(p, ec);

	if (ec)
	{
		if (ec.value() == ERROR_ACCESS_DENIED)
		{
			cx.trace(
				context::fs,
				"got access denied trying to delete dir " + p.string() + ", "
				"trying to remove read-only flag recursively");

			remove_readonly(cx, p);
			fs::remove_all(p, ec);

			if (!ec)
				return;
		}

		cx.bail_out(context::fs, "failed to delete " + p.string(), ec);
	}
}

void do_delete_file(const context& cx, const fs::path& p)
{
	std::error_code ec;
	fs::remove(p, ec);

	if (ec)
		cx.bail_out(context::fs, "can't delete " + p.string(), ec);
}

void do_copy_file_to_dir(
	const context& cx, const fs::path& f, const fs::path& d)
{
	op::create_directories(cx, d);

	std::error_code ec;
	fs::copy_file(
		f, d / f.filename(),
		fs::copy_options::overwrite_existing, ec);

	if (ec)
	{
		cx.bail_out(context::fs,
			"can't copy " + f.string() + " to " + d.string(), ec);
	}
}

void do_remove_readonly(const context& cx, const fs::path& p)
{
	cx.trace(context::fs, "chmod +x " + p.string());

	std::error_code ec;
	fs::permissions(p, fs::perms::owner_write, fs::perm_options::add, ec);

	if (ec)
	{
		cx.bail_out(context::fs,
			"can't remove read-only flag on " + p.string(), ec);
	}
}

void do_rename(const context& cx, const fs::path& src, const fs::path& dest)
{
	std::error_code ec;
	fs::rename(src, dest, ec);

	if (ec)
	{
		cx.bail_out(context::fs,
			"can't rename " + src.string() + " to " + dest.string(), ec);
	}
}

void check(const context& cx, const fs::path& p)
{
	if (p.empty())
		cx.bail_out(context::fs, "path is empty");

	auto is_inside = [](auto&& p, auto&& dir)
	{
		const std::string s = p.string();
		const std::string prefix = dir.string();

		if (s.size() < prefix.size())
			return false;

		const std::string scut = s.substr(0, prefix.size());

		if (_stricmp(scut.c_str(), prefix.c_str()) != 0)
			return false;

		return true;
	};

	if (is_inside(p, paths::prefix()))
		return;

	if (is_inside(p, paths::temp_dir()))
		return;

	if (is_inside(p, paths::licenses()))
		return;

	cx.bail_out(context::fs, "path " + p.string() + " is outside prefix");
}

}	// namespace

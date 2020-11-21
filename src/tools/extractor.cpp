#include "pch.h"
#include "tools.h"
#include "../core/process.h"

namespace mob
{

extractor::extractor()
	: basic_process_runner("extract")
{
}

fs::path extractor::binary()
{
	return conf::tool_by_name("sevenz");
}

extractor& extractor::file(const fs::path& file)
{
	file_ = file;
	return *this;
}

extractor& extractor::output(const fs::path& dir)
{
	where_ = dir;
	return *this;
}

void extractor::do_run()
{
	interruption_file ifile(cx(), where_, "extractor");

	if (ifile.exists())
	{
		cx().debug(context::generic,
			"previous extraction was interrupted; resuming");
	}
	else if (fs::exists(where_))
	{
		if (conf().global().reextract())
		{
			cx().debug(context::reextract, "deleting {}", where_);
			op::delete_directory(cx(), where_, op::optional);
		}
		else
		{
			cx().debug(context::bypass, "directory {} already exists", where_);
			return;
		}
	}

	cx().debug(context::generic, "extracting {} into {}", file_, where_);

	ifile.create();

	op::create_directories(cx(), where_);
	directory_deleter delete_output(cx(), where_);

	// the -spe from 7z is supposed to figure out if there's a folder in the
	// archive with the same name as the target and extract its content to
	// avoid duplicating the folder
	//
	// however, it fails miserably if there are files along with that folder,
	// which is the case for openssl:
	//
	//  openssl-1.1.1d.tar/
	//   +- openssl-1.1.1d/
	//   +- pax_global_header
	//
	// that pax_global_header makes 7z fail with "unspecified error"
	//
	// so the handling of a duplicate directory is done manually in
	// check_duplicate_directory() below

	if (file_.u8string().ends_with(u8".tar.gz"))
	{
		cx().trace(context::generic, "this is a tar.gz, piping");

		auto extract_tar = process()
			.binary(binary())
			.arg("x")
			.arg("-so", file_);

		auto extract_gz = process()
			.binary(binary())
			.arg("x")
			.arg("-aoa")
			.arg("-si")
			.arg("-ttar")
			.arg("-o", where_, process::nospace);

		set_process(process::pipe(extract_tar, extract_gz));
	}
	else
	{
		set_process(process()
			.binary(binary())
			.arg("x")
			.arg("-aoa")
			.arg("-bd")
			.arg("-bb0")
			.arg("-o", where_, process::nospace)
			.arg(file_));
	}

	execute_and_join();
	check_duplicate_directory(ifile.file());

	delete_output.cancel();

	if (!interrupted())
		ifile.remove();
}

void extractor::check_duplicate_directory(const fs::path& ifile)
{
	const auto dir_name = where_.filename();

	// check for a folder with the same name
	if (!fs::exists(where_ / dir_name))
	{
		cx().trace(context::generic,
			"no duplicate subdir {}, leaving as-is", dir_name);

		return;
	}

	cx().trace(context::generic,
		"found subdir {} with same name as output dir; "
		"moving everything up one",
		dir_name);

	// the archive contained a directory with the same name as the output
	// directory

	// delete anything other than this directory; some archives have
	// useless files along with it
	for (auto e : fs::directory_iterator(where_))
	{
		// but don't delete the directory itself
		if (e.path().filename() == dir_name)
			continue;

		// or the interrupt file
		if (e.path().filename() == ifile.filename())
			continue;

		if (!fs::is_regular_file(e.path()))
		{
			// don't know what to do with archives that have the
			// same directory _and_ other directories
			cx().bail_out(context::generic,
				"check_duplicate_directory: {} is yet another directory",
				e.path());
		}

		cx().trace(context::generic,
			"assuming file {} is useless, deleting", e.path());

		op::delete_file(cx(), e.path());
	}

	// now there should only be two things in this directory: another
	// directory with the same name and the interrupt file

	// give it a temp name in case there's yet another directory with the
	// same name in it
	const auto temp_dir = where_ / (u8"_mob_" + dir_name.u8string());

	cx().trace(context::generic,
		"renaming dir to {} to avoid clashes", temp_dir);

	if (fs::exists(temp_dir))
	{
		cx().trace(context::generic,
			"temp dir {} already exists, deleting", temp_dir);

		op::delete_directory(cx(), temp_dir);
	}

	op::rename(cx(), where_ / dir_name, temp_dir);

	// move the content of the directory up
	for (auto e : fs::directory_iterator(temp_dir))
		op::move_to_directory(cx(), e.path(), where_);

	// delete the old directory, which should be empty now
	op::delete_directory(cx(), temp_dir);
}


void archiver::create_from_glob(
	const context& cx, const fs::path& out,
	const fs::path& glob, const std::vector<std::string>& ignore)
{
	op::create_directories(cx, out.parent_path());

	auto p = process()
		.binary(extractor::binary())
		.arg("a")
		.arg(out)
		.arg("-r")
		.arg("-mx=5")
		.arg(glob);

	for (auto&& i : ignore)
		p.arg("-xr!", i, process::nospace);

	p.run();
	p.join();
}

void archiver::create_from_files(
	const context& cx, const fs::path& out,
	const std::vector<fs::path>& files, const fs::path& files_root)
{
	std::string list_file_text;
	std::error_code ec;

	for (auto&& f : files)
	{
		fs::path rf = fs::relative(f, files_root, ec);

		if (ec)
		{
			cx.bail_out(context::fs,
				"file {} is not in root {}", f, files_root);
		}

		list_file_text += path_to_utf8(rf) + "\n";
	}

	const auto list_file = make_temp_file();
	guard g([&]
	{
		if (fs::exists(list_file))
		{
			std::error_code ec;
			fs::remove(list_file, ec);
		}
	});

	op::write_text_file(gcx(), encodings::utf8, list_file, list_file_text);
	op::create_directories(cx, out.parent_path());

	auto p = process()
		.binary(extractor::binary())
		.arg("a")
		.arg(out)
		.arg("@", list_file, process::nospace)
		.cwd(files_root);

	p.run();
	p.join();
}

}	// namespace

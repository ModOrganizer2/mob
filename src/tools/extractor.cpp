#include "pch.h"
#include "tools.h"

namespace mob
{

extractor::extractor()
	: basic_process_runner("extract")
{
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
	interruption_file ifile(*cx_, where_, "extractor");

	if (ifile.exists())
	{
		cx_->debug(context::generic,
			"previous extraction was interrupted; resuming");
	}
	else if (fs::exists(where_))
	{
		if (conf::reextract())
		{
			cx_->debug(context::reextract, "deleting " + where_.string());
			op::delete_directory(*cx_, where_, op::optional);
		}
		else
		{
			cx_->debug(
				context::bypass,
				"directory " + where_.string() + " already exists");

			return;
		}
	}

	cx_->debug(context::generic,
		"extracting " + file_.string() + " into " + where_.string());

	ifile.create();

	op::create_directories(*cx_, where_);
	directory_deleter delete_output(*cx_, where_);

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

	if (file_.string().ends_with(".tar.gz"))
	{
		cx_->trace(context::generic, "this is a tar.gz, piping");

		auto extract_tar = process()
			.binary(third_party::sevenz())
			.arg("x")
			.arg("-so", file_);

		auto extract_gz = process()
			.binary(third_party::sevenz())
			.arg("x")
			.arg("-aoa")
			.arg("-si")
			.arg("-ttar")
			.arg("-o", where_, process::nospace);

		process_ = process::pipe(extract_tar, extract_gz);
	}
	else
	{
		process_ = process()
			.binary(third_party::sevenz())
			.arg("x")
			.arg("-aoa")
			.arg("-bd")
			.arg("-bb0")
			.arg("-o", where_, process::nospace)
			.arg(file_);
	}

	execute_and_join();
	check_duplicate_directory(ifile.file());

	delete_output.cancel();

	if (!interrupted())
		ifile.remove();
}

void extractor::check_duplicate_directory(const fs::path& ifile)
{
	const auto dir_name = where_.filename().string();

	// check for a folder with the same name
	if (!fs::exists(where_ / dir_name))
	{
		cx_->trace(context::generic,
			"no duplicate subdir " + dir_name + ", leaving as-is");

		return;
	}

	cx_->trace(context::generic,
		"found subdir " + dir_name + " with same name as output dir; "
		"moving everything up one");

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
			cx_->bail_out(context::generic,
				"check_duplicate_directory: " + e.path().string() + " is "
				"yet another directory");
		}

		cx_->trace(context::generic,
			"assuming file " + e.path().string() + " is useless, deleting");

		op::delete_file(*cx_, e.path());
	}

	// now there should only be two things in this directory: another
	// directory with the same name and the interrupt file

	// give it a temp name in case there's yet another directory with the
	// same name in it
	const auto temp_dir = where_ / ("_mob_" + dir_name );

	cx_->trace(context::generic,
		"renaming dir to " + temp_dir.string() + " to avoid clashes");

	if (fs::exists(temp_dir))
	{
		cx_->trace(context::generic,
			"temp dir " + temp_dir.string() + " already exists, "
			"deleting");

		op::delete_directory(*cx_, temp_dir);
	}

	op::rename(*cx_, where_ / dir_name, temp_dir);

	// move the content of the directory up
	for (auto e : fs::directory_iterator(temp_dir))
		op::move_to_directory(*cx_, e.path(), where_);

	// delete the old directory, which should be empty now
	op::delete_directory(*cx_, temp_dir);
}

}	// namespace

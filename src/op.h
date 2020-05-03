#pragma once

#include "utility.h"

namespace mob { class context; }

namespace mob::op
{

enum flags
{
	noflags = 0,
	optional = 1
};

void touch(const fs::path& p, const context* cx=nullptr);

void create_directories(
	const fs::path& p, const context* cx=nullptr);

void delete_directory(
	const fs::path& p, flags f=noflags, const context* cx=nullptr);

void delete_file(
	const fs::path& p, flags f=noflags, const context* cx=nullptr);

void remove_readonly(
	const fs::path& first, const context* cx=nullptr);

void rename(
	const fs::path& src, const fs::path& dest, const context* cx=nullptr);

void move_to_directory(
	const fs::path& src, const fs::path& dest_dir, const context* cx=nullptr);

void copy_file_to_dir_if_better(
	const fs::path& file, const fs::path& dest_dir, flags f=noflags,
	const context* cx=nullptr);

}	// namespace

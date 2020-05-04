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

void touch(const context& cx, const fs::path& p);

void create_directories(
	const context& cx, const fs::path& p);

void delete_directory(
	const context& cx, const fs::path& p, flags f=noflags);

void delete_file(
	const context& cx, const fs::path& p, flags f=noflags);

void remove_readonly(
	const context& cx, const fs::path& first);

void rename(
	const context& cx, const fs::path& src, const fs::path& dest);

void move_to_directory(
	const context& cx, const fs::path& src, const fs::path& dest_dir);

void copy_file_to_dir_if_better(
	const context& cx,
	const fs::path& file, const fs::path& dest_dir, flags f=noflags);

}	// namespace

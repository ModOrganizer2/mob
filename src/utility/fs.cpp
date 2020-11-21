#include "pch.h"
#include "fs.h"
#include "../utility.h"
#include "../core/context.h"
#include "../core/conf.h"
#include "../core/op.h"

namespace mob
{

file_deleter::file_deleter(const context& cx, fs::path p)
	: cx_(cx), p_(std::move(p)), delete_(true)
{
	cx_.trace(context::fs, "will delete {} if things go bad", p_);
}

file_deleter::~file_deleter()
{
	if (delete_)
		delete_now();
}

void file_deleter::delete_now()
{
	cx_.debug(context::fs, "something went bad, deleting {}", p_);
	op::delete_file(cx_, p_, op::optional);
}

void file_deleter::cancel()
{
	cx_.trace(context::fs, "everything okay, keeping {}", p_);
	delete_ = false;
}


directory_deleter::directory_deleter(const context& cx, fs::path p)
	: cx_(cx), p_(std::move(p)), delete_(true)
{
	cx_.trace(context::fs, "will delete {} if things go bad", p_);
}

directory_deleter::~directory_deleter()
{
	if (delete_)
		delete_now();

}

void directory_deleter::delete_now()
{
	cx_.debug(context::fs, "something went bad, deleting {}", p_);
	op::delete_directory(cx_, p_, op::optional);
}

void directory_deleter::cancel()
{
	cx_.trace(context::fs, "everything okay, keeping {}", p_);
	delete_ = false;
}


interruption_file::interruption_file(
	const context& cx, fs::path dir, std::string name)
		: cx_(cx), dir_(std::move(dir)), name_(std::move(name))
{
	if (fs::exists(file()))
		cx_.trace(context::interruption, "found interrupt file {}", file());
}

bool interruption_file::exists() const
{
	return fs::exists(file());
}

fs::path interruption_file::file() const
{
	return dir_ / ("_mo_interrupted_" + name_);
}

void interruption_file::create()
{
	cx_.trace(context::interruption, "creating interrupt file {}", file());
	op::touch(cx_, file());
}

void interruption_file::remove()
{
	cx_.trace(context::interruption, "removing interrupt file {}", file());
	op::delete_file(cx_, file());
}


bypass_file::bypass_file(const context& cx, fs::path dir, std::string name)
	: cx_(cx), file_(dir / ("_mob_" + name))
{
}

bool bypass_file::exists() const
{
	if (fs::exists(file_))
	{
		if (conf().global().rebuild())
		{
			cx_.trace(context::rebuild,
				"bypass file {} exists, deleting", file_);

			op::delete_file(cx_, file_, op::optional);

			return false;
		}
		else
		{
			cx_.trace(context::bypass, "bypass file {} exists", file_);
			return true;
		}
	}
	else
	{
		cx_.trace(context::bypass, "bypass file {} not found", file_);
		return false;
	}
}

void bypass_file::create()
{
	cx_.trace(context::bypass, "create bypass file {}", file_);
	op::touch(cx_, file_);
}

}	// namespace

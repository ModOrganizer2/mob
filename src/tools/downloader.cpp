#include "pch.h"
#include "tools.h"

namespace mob
{

downloader::downloader(ops o)
	: tool("dl"), op_(o)
{
}

downloader::downloader(mob::url u, ops o)
	: downloader(o)
{
	urls_.push_back(std::move(u));
}

downloader& downloader::url(const mob::url& u)
{
	urls_.push_back(u);
	return *this;
}

downloader& downloader::file(const fs::path& p)
{
	file_ = p;
	return *this;
}

fs::path downloader::result() const
{
	return file_;
}

void downloader::do_run()
{
	switch (op_)
	{
		case clean:
		{
			do_clean();
			break;
		}

		case download:
		{
			do_download();
			break;
		}

		default:
		{
			cx().bail_out(context::net, "bad downloader op {}", op_);
		}
	}
}

void downloader::do_download()
{
	dl_.reset(new curl_downloader(&cx()));

	cx().trace(context::net, "looking for already downloaded files");

	if (!file_.empty())
	{
		if (try_picking(file_))
			return;
	}
	else
	{
		for (auto&& u : urls_)
		{
			const auto file = path_for_url(u);

			if (try_picking(file))
			{
				file_ = file;
				return;
			}
		}
	}


	cx().trace(context::net, "no cached downloads were found, will try:");
	for (auto&& u : urls_)
		cx().trace(context::net, "  . {}", u);


	// try them in order
	for (auto&& u : urls_)
	{
		if (file_.empty())
			file_ = path_for_url(u);

		cx().trace(context::net, "trying {} into {}", u, file_);

		dl_->start(u, file_);
		cx().trace(context::net, "waiting for download");
		dl_->join();

		if (dl_->ok())
		{
			cx().trace(context::net, "file {} downloaded", file_);
			return;
		}

		cx().debug(context::net, "download failed");
	}

	if (interrupted())
	{
		cx().trace(context::interruption, "interrupted");
		return;
	}

	// all failed
	cx().bail_out(context::net, "all urls failed to download");
}

void downloader::do_clean()
{
	if (!file_.empty())
	{
		cx().debug(context::redownload, "deleting {}", file_);
		op::delete_file(cx(), file_, op::optional);
	}
	else
	{
		for (auto&& u : urls_)
		{
			const auto file = path_for_url(u);

			cx().debug(context::redownload, "deleting {}", file);
			op::delete_file(cx(), file, op::optional);
		}
	}
}

void downloader::do_interrupt()
{
	if (dl_)
		dl_->interrupt();
}

bool downloader::try_picking(const fs::path& file)
{
	if (fs::exists(file))
	{
		cx().trace(context::bypass, "picking {}", file_);
		return true;
	}
	else
	{
		cx().trace(context::net, "no {}", file);
	}

	return false;
}

fs::path downloader::path_for_url(const mob::url& u) const
{
	std::string filename;

	std::string url_string = u.string();

	if (url_string.find("sourceforge.net") != std::string::npos)
	{
		// sf downloads end with /download, strip it to get the filename
		const std::string strip = "/download";

		cx().trace(context::net,
			"url {} is sourceforge, stripping {} for filename", u, strip);

		if (url_string.ends_with(strip))
			url_string = url_string.substr(0, url_string.size() - strip.size());
		else
			cx().trace(context::net, "no need to strip {}", u);

		filename = mob::url(url_string).filename();
	}
	else
	{
		filename = u.filename();
	}

	return paths::cache() / filename;
}

}	// namespace

#include "pch.h"
#include "tools.h"

namespace mob
{

downloader::downloader()
	: tool("dl")
{
}

downloader::downloader(mob::url u)
	: downloader()
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
	dl_.reset(new curl_downloader(cx_));

	cx_->trace(context::net, "looking for already downloaded files");

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


	cx_->trace(context::net, "no cached downloads were found, will try:");
	for (auto&& u : urls_)
		cx_->trace(context::net, "  . {}", u);


	// try them in order
	for (auto&& u : urls_)
	{
		if (file_.empty())
			file_ = path_for_url(u);

		cx_->trace(context::net, "trying {} into {}", u, file_);

		dl_->start(u, file_);
		cx_->trace(context::net, "waiting for download");
		dl_->join();

		if (dl_->ok())
		{
			cx_->trace(context::net, "file {} downloaded", file_);
			return;
		}

		cx_->debug(context::net, "download failed");
	}

	if (interrupted())
	{
		cx_->trace(context::interruption, "");
		return;
	}

	// all failed
	cx_->bail_out(context::net, "all urls failed to download");
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
		if (conf::redownload())
		{
			cx_->trace(context::redownload, "deleting {}", file);
			op::delete_file(*cx_, file, op::optional);
		}
		else
		{
			cx_->trace(context::bypass, "picking {}", file_);
			return true;
		}
	}
	else
	{
		cx_->trace(context::net, "no {}", file);
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

		cx_->trace(context::net,
			"url {} is sourceforge, stripping {} for filename", u, strip);

		if (url_string.ends_with(strip))
			url_string = url_string.substr(0, url_string.size() - strip.size());
		else
			cx_->trace(context::net, "no need to strip {}", u);

		filename = mob::url(url_string).filename();
	}
	else
	{
		filename = u.filename();
	}

	return paths::cache() / filename;
}

}	// namespace

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
	cx_->log(context::trace, "adding url " + u.string());
	urls_.push_back(u);

	return *this;
}

fs::path downloader::result() const
{
	return file_;
}

void downloader::do_run()
{
	dl_.reset(new curl_downloader(cx_));

	cx_->log(context::trace, "looking for already downloaded files");

	for (auto&& u : urls_)
	{
		const auto file = path_for_url(u);

		if (fs::exists(file))
		{
			if (conf::redownload())
			{
				cx_->log(context::redownload, "deleting " + file.string());
				op::delete_file(file, op::optional, cx_);
			}
			else
			{
				cx_->log(context::trace, "picking " + file_.string());
				file_ = file;
				return;
			}
		}
		else
		{
			cx_->log(context::trace, "no " + file.string());
		}
	}


	cx_->log(context::trace, "no cached downloads were found, will try:");
	for (auto&& u : urls_)
		cx_->log(context::trace, "  . " + u.string());


	// try them in order
	for (auto&& u : urls_)
	{
		const fs::path file = path_for_url(u);

		cx_->log(
			context::trace,
			"trying " + u.string() + " into " + file.string());

		dl_->start(u, file);
		cx_->log(context::trace, "waiting for download");
		dl_->join();

		if (dl_->ok())
		{
			cx_->log(context::trace, "file " + file.string() + " downloaded");
			file_ = file;
			return;
		}

		cx_->log(context::trace, "download failed");
	}

	if (interrupted())
	{
		cx_->log(context::interrupted, "");
		return;
	}

	// all failed
	cx_->bail_out("all urls failed to download");
}

void downloader::do_interrupt()
{
	if (dl_)
		dl_->interrupt();
}

fs::path downloader::path_for_url(const mob::url& u) const
{
	std::string filename;

	std::string url_string = u.string();

	if (url_string.find("sourceforge.net") != std::string::npos)
	{
		// sf downloads end with /download, strip it to get the filename
		const std::string strip = "/download";

		cx_->log(
			context::trace,
			"url " + u.string() + " is a sourceforge download, stripping " +
			strip);

		if (url_string.ends_with(strip))
			url_string = url_string.substr(0, url_string.size() - strip.size());
		else
			cx_->log(context::trace, "no need to strip " + u.string());

		filename = mob::url(url_string).filename();
	}
	else
	{
		filename = u.filename();
	}

	return paths::cache() / filename;
}

}	// namespace

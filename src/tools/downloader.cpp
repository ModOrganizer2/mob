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
	cx_->trace(context::net, "adding url " + u.string());
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

	cx_->trace(context::net, "looking for already downloaded files");

	for (auto&& u : urls_)
	{
		const auto file = path_for_url(u);

		if (fs::exists(file))
		{
			if (conf::redownload())
			{
				cx_->trace(context::redownload, "deleting " + file.string());
				op::delete_file(*cx_, file, op::optional);
			}
			else
			{
				cx_->trace(context::bypass, "picking " + file_.string());
				file_ = file;
				return;
			}
		}
		else
		{
			cx_->trace(context::net, "no " + file.string());
		}
	}


	cx_->trace(context::net, "no cached downloads were found, will try:");
	for (auto&& u : urls_)
		cx_->trace(context::net, "  . " + u.string());


	// try them in order
	for (auto&& u : urls_)
	{
		const fs::path file = path_for_url(u);

		cx_->debug(context::net,
			"trying " + u.string() + " into " + file.string());

		dl_->start(u, file);
		cx_->trace(context::net, "waiting for download");
		dl_->join();

		if (dl_->ok())
		{
			cx_->trace(context::net, "file " + file.string() + " downloaded");
			file_ = file;
			return;
		}

		cx_->debug(context::net, "download failed");
	}

	if (interrupted())
	{
		cx_->trace(context::interrupted, "");
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

fs::path downloader::path_for_url(const mob::url& u) const
{
	std::string filename;

	std::string url_string = u.string();

	if (url_string.find("sourceforge.net") != std::string::npos)
	{
		// sf downloads end with /download, strip it to get the filename
		const std::string strip = "/download";

		cx_->trace(context::net,
			"url " + u.string() + " is sourceforge, stripping " + strip);

		if (url_string.ends_with(strip))
			url_string = url_string.substr(0, url_string.size() - strip.size());
		else
			cx_->trace(context::net, "no need to strip " + u.string());

		filename = mob::url(url_string).filename();
	}
	else
	{
		filename = u.filename();
	}

	return paths::cache() / filename;
}

}	// namespace

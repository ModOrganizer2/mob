#include "pch.h"
#include "tools.h"

namespace mob {

    downloader::downloader(ops o) : tool("dl"), op_(o) {}

    downloader::downloader(mob::url u, ops o) : downloader(o)
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
        switch (op_) {
        case clean: {
            do_clean();
            break;
        }

        case download: {
            do_download();
            break;
        }

        default: {
            cx().bail_out(context::net, "bad downloader op {}", op_);
        }
        }
    }

    void downloader::do_download()
    {
        dl_.reset(new curl_downloader(&cx()));

        cx().trace(context::net, "looking for already downloaded files");
        if (use_existing()) {
            cx().trace(context::bypass, "using {}", file_);
            return;
        }

        cx().trace(context::net, "no cached downloads were found, will try:");
        for (auto&& u : urls_)
            cx().trace(context::net, "  . {}", u);

        // try them in order
        for (auto&& u : urls_) {
            if (try_download(u)) {
                // done
                return;
            }
        }

        if (interrupted()) {
            cx().trace(context::interruption, "interrupted");
            return;
        }

        // all failed
        cx().bail_out(context::net, "all urls failed to download");
    }

    bool downloader::try_download(const mob::url& u)
    {
        // when file() wasn't called, the output file is created from the url
        if (file_.empty())
            file_ = path_for_url(u);

        // downloading
        cx().trace(context::net, "trying {} into {}", u, file_);
        dl_->start(u, file_);

        cx().trace(context::net, "waiting for download");
        dl_->join();

        if (dl_->ok()) {
            // done
            cx().trace(context::net, "file {} downloaded", file_);
            return true;
        }

        cx().debug(context::net, "download failed");
        return false;
    }

    void downloader::do_clean()
    {
        if (file_.empty()) {
            // file() wasn't called, delete all the files that would be created
            // depending on the urls given

            for (auto&& u : urls_) {
                const auto file = path_for_url(u);

                cx().debug(context::redownload, "deleting {}", file);
                op::delete_file(cx(), file, op::optional);
            }
        }
        else {
            // delete the given output file
            cx().debug(context::redownload, "deleting {}", file_);
            op::delete_file(cx(), file_, op::optional);
        }
    }

    void downloader::do_interrupt()
    {
        if (dl_)
            dl_->interrupt();
    }

    bool downloader::use_existing()
    {
        if (file_.empty()) {
            // check if one of the files that would be created by a url exists
            for (auto&& u : urls_) {
                const auto file = path_for_url(u);

                if (fs::exists(file)) {
                    // take it
                    file_ = file;
                    return true;
                }
            }
        }
        else {
            // file() was called, check if it exists
            if (fs::exists(file_))
                return true;
        }

        return false;
    }

    fs::path downloader::path_for_url(const mob::url& u) const
    {
        std::string filename;

        std::string url_string = u.string();

        if (url_string.find("sourceforge.net") != std::string::npos) {
            // sf downloads end with /download, strip it to get the filename
            const std::string strip = "/download";

            cx().trace(context::net, "url {} is sourceforge, stripping {} for filename",
                       u, strip);

            if (url_string.ends_with(strip))
                url_string = url_string.substr(0, url_string.size() - strip.size());
            else
                cx().trace(context::net, "no need to strip {}", u);

            filename = mob::url(url_string).filename();
        }
        else {
            filename = u.filename();
        }

        // downloaded files go in the cache, typically build/downloads/
        return conf().path().cache() / filename;
    }

}  // namespace mob

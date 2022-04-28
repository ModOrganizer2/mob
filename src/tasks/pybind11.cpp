#include "pch.h"
#include "tasks.h"
#include "../core/process.h"

// see the top of pyqt.cpp for some stuff about python/sip/pyqt

namespace mob::tasks
{

	pybind11::pybind11() : basic_task("pybind11") { }

	bool pybind11::prebuilt()
	{
		// header only available
		return true;
	}

	std::string pybind11::version()
	{
		return conf().version().get("pybind11");
	}

	fs::path pybind11::source_path()
	{
		return conf().path().build() / "pybind11";
	}

	void pybind11::do_clean(clean c)
	{
		if (is_set(c, clean::reclone))
		{
			git_wrap::delete_directory(cx(), source_path());
			return;
		}
	}
	
	void pybind11::do_fetch()
	{
		run_tool(make_git()
			.url(make_git_url("pybind", "pybind11"))
			.branch(version())
			.root(source_path()));

	}
	
	void pybind11::do_build_and_install()
	{

	}
}
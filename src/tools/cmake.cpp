#include "pch.h"
#include "tools.h"

namespace mob
{

cmake::cmake()
	: basic_process_runner("cmake"), gen_(jom), arch_(arch::def)
{
	process_
		.binary(binary());
}

fs::path cmake::binary()
{
	return tool_by_name("cmake");
}

void cmake::clean(const context& cx, const fs::path& root)
{
	cx.trace(context::rebuild, "deleting all generator directories");

	for (auto&& [k, g] : all_generators())
	{
		op::delete_directory(cx,
			root / g.output_dir(arch::x86), op::optional);

		op::delete_directory(cx,
			root / g.output_dir(arch::x64), op::optional);
	}
}

cmake& cmake::generator(generators g)
{
	gen_ = g;
	return *this;
}

cmake& cmake::root(const fs::path& p)
{
	root_ = p;
	return *this;
}

cmake& cmake::prefix(const fs::path& s)
{
	prefix_ = s;
	return *this;
}

cmake& cmake::def(const std::string& name, const std::string& value)
{
	process_.arg("-D" + name + "=" + value + "");
	return *this;
}

cmake& cmake::def(const std::string& name, const fs::path& p)
{
	def(name, path_to_utf8(p));
	return *this;
}

cmake& cmake::def(const std::string& name, const char* s)
{
	def(name, std::string(s));
	return *this;
}

cmake& cmake::architecture(arch a)
{
	arch_ = a;
	return *this;
}

fs::path cmake::result() const
{
	return output_;
}

void cmake::do_run()
{
	if (root_.empty())
		cx_->bail_out(context::generic, "cmake output path is empty");

	const auto& g = get_generator(gen_);
	output_ = root_ / (g.output_dir(arch_));

	process_
		.stdout_encoding(encodings::utf8)
		.stderr_encoding(encodings::utf8)
		.arg("-G", "\"" + g.name + "\"")
		.arg("-DCMAKE_BUILD_TYPE=Release")
		.arg("-DCMAKE_INSTALL_MESSAGE=NEVER", process::log_quiet)
		.arg("--log-level", "WARNING", process::log_quiet)
		.arg("--no-warn-unused-cli")
		.arg(g.get_arch(arch_));

	if (!prefix_.empty())
		process_.arg("-DCMAKE_INSTALL_PREFIX=", prefix_, process::nospace);

	process_
		.arg("..")
		.env(env::vs(arch_)
			.set("CXXFLAGS", "/wd4566"))
		.cwd(output_);

	execute_and_join();
}

const std::map<cmake::generators, cmake::gen_info>& cmake::all_generators()
{
	static const std::map<generators, gen_info> map =
	{
		{ generators::jom, {"build", "NMake Makefiles JOM", "", "" }},

		{ generators::vs, {
			"vsbuild",
			"Visual Studio " + vs::version() + " " + vs::year(),
			"Win32",
			"x64"
	}}
	};

	return map;
}

const cmake::gen_info& cmake::get_generator(generators g)
{
	const auto& map = all_generators();

	auto itor = map.find(g);
	if (itor == map.end())
		bail_out("unknown generator");

	return itor->second;
}

std::string cmake::gen_info::get_arch(arch a) const
{
	switch (a)
	{
		case arch::x86:
		{
			if (x86.empty())
				return {};
			else
				return "-A " + x86;
		}

		case arch::x64:
		{
			if (x64.empty())
				return {};
			else
				return "-A " + x64;
		}

		case arch::dont_care:
			return {};

		default:
			bail_out("gen_info::get_arch(): bad arch");
	}
}

std::string cmake::gen_info::output_dir(arch a) const
{
	switch (a)
	{
		case arch::x86:
			return (dir + "_32");

		case arch::x64:
		case arch::dont_care:
			return dir;

		default:
			bail_out("gen_info::get_arch(): bad arch");
	}
}

}	// namespace

#include "pch.h"
#include "tools.h"

namespace mob
{

cmake::cmake()
	: basic_process_runner("cmake"), gen_(jom), arch_(arch::def)
{
	process_
		.binary(third_party::cmake());
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

cmake& cmake::def(const std::string& s)
{
	process_.arg("-D" + s);
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
	if (conf::rebuild())
	{
		for (auto&& [k, g] : all_generators())
		{
			op::delete_directory(*cx_,
				root_ / g.output_dir(arch::x86), op::optional);

			op::delete_directory(*cx_,
				root_ / g.output_dir(arch::x64), op::optional);
		}
	}

	const auto& g = get_generator();
	output_ = root_ / (g.output_dir(arch_));

	process_
		.arg("-G", "\"" + g.name + "\"")
		.arg("-DCMAKE_BUILD_TYPE=Release")
		.arg("-DCMAKE_INSTALL_MESSAGE=NEVER", process::quiet)
		.arg("--log-level", "WARNING", process::quiet)
		.arg(g.get_arch(arch_));

	if (!prefix_.empty())
		process_.arg("-DCMAKE_INSTALL_PREFIX=", prefix_, process::nospace);

	process_
		.arg("..")
		.env(env::vs(arch_))
		.cwd(output_);

	execute_and_join();
}

const std::map<cmake::generators, cmake::gen_info>&
cmake::all_generators() const
{
	static const std::map<generators, gen_info> map =
	{
		{ generators::jom, {"build", "NMake Makefiles JOM", "", "" }},

		{ generators::vs, {
			"vsbuild",
			"Visual Studio " + versions::vs() + " " + versions::vs_year(),
			"Win32",
			"x64"
	}}
	};

	return map;
}

const cmake::gen_info& cmake::get_generator() const
{
	const auto& map = all_generators();

	auto itor = map.find(gen_);
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
				return "-A" + x64;
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

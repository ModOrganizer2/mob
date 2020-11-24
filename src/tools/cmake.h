#pragma once

namespace mob
{

class cmake : public basic_process_runner
{
public:
	enum generators
	{
		vs    = 0x01,
		jom   = 0x02
	};

	enum ops
	{
		generate =1 ,
		clean
	};

	cmake(ops o=generate);

	static fs::path binary();

	cmake& generator(generators g);
	cmake& generator(const std::string& g);
	cmake& root(const fs::path& p);
	cmake& output(const fs::path& p);
	cmake& prefix(const fs::path& s);
	cmake& def(const std::string& name, const std::string& value);
	cmake& def(const std::string& name, const fs::path& p);
	cmake& def(const std::string& name, const char* s);
	cmake& architecture(arch a);
	cmake& cmd(const std::string& s);

	// returns the path given in output(), if it was set
	//
	// if not, returns the build path based on the parameters (for example,
	// `vsbuild_32/` for a 32-bit arch with the VS generator
	//
	fs::path build_path() const;

	// returns build_path(), used by task::run_tool()
	//
	fs::path result() const;

protected:
	void do_run() override;

private:
	struct gen_info
	{
		std::string dir;
		std::string name;
		std::string x86;
		std::string x64;

		std::string get_arch(arch a) const;
		std::string output_dir(arch a) const;
	};

	ops op_;
	fs::path root_;
	generators gen_;
	std::string genstring_;
	fs::path prefix_;
	std::vector<std::pair<std::string, std::string>> def_;
	fs::path output_;
	arch arch_;
	std::string cmd_;

	void do_clean();
	void do_generate();

	static const std::map<generators, gen_info>& all_generators();
	static const gen_info& get_generator(generators g);
};

}	// namespace

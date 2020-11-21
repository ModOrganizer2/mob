#pragma once

namespace mob
{

std::string default_ini_filename();
void parse_ini(const fs::path& ini, bool add);

struct parsed_option
{
	std::string task, section, key, value;
};

parsed_option parse_option(const std::string& s);

}	// namespace

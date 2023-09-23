#pragma once

namespace mob {

    std::string default_ini_filename();

    std::vector<fs::path>
    find_inis(bool auto_detect, const std::vector<std::string>& from_cl, bool verbose);

    struct ini_data {
        using alias_patterns = std::vector<std::string>;
        using aliases_map    = std::map<std::string, alias_patterns>;

        using kv_map          = std::map<std::string, std::string>;
        using sections_vector = std::vector<std::pair<std::string, kv_map>>;

        fs::path path;
        aliases_map aliases;
        sections_vector sections;

        kv_map& get_section(std::string_view name);
        void set(std::string_view section, std::string key, std::string value);
    };

    ini_data parse_ini(const fs::path& ini);
    std::string default_ini_filename();

}  // namespace mob

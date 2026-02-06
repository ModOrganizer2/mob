file(GLOB_RECURSE source_files CONFIGURE_DEPENDS "*.cpp")
file(GLOB_RECURSE header_files CONFIGURE_DEPENDS "*.h")

add_executable(mob ${source_files} ${header_files})

target_compile_features(mob PRIVATE cxx_std_20)

target_compile_definitions(
  mob PRIVATE _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000007 WIN32_LEAN_AND_MEAN
              NOMINMAX NOCOMM)

target_link_libraries(mob PRIVATE clipp::clipp nlohmann_json::nlohmann_json
                                  CURL::libcurl dbghelp shlwapi version)

source_group(
  TREE ${CMAKE_CURRENT_SOURCE_DIR}
  PREFIX src
  FILES ${source_files} ${header_files})

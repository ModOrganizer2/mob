cmake_minimum_required(VERSION 3.16)

file(GLOB_RECURSE src_files *.cpp)

add_executable(mob ${src_files})
set_target_properties(mob PROPERTIES CXX_STANDARD 20)

target_compile_definitions(mob PUBLIC NOMINMAX)
target_compile_options(mob PUBLIC "/MT")
target_include_directories(mob PUBLIC ${CMAKE_SOURCE_DIR}/third-party/include)
target_link_libraries(mob PUBLIC
    wsock32 ws2_32 crypt32 wldap32 dbghelp shlwapi version
    optimized ${CMAKE_SOURCE_DIR}/third-party/lib/libcurl.lib
    debug ${CMAKE_SOURCE_DIR}/third-party/lib/libcurl-d.lib
    optimized ${CMAKE_SOURCE_DIR}/third-party/lib/zlib.lib
    debug ${CMAKE_SOURCE_DIR}/third-party/lib/zlibd.lib)

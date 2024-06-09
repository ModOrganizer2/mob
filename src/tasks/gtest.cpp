#include "pch.h"
#include "tasks.h"

namespace mob::tasks {

    namespace {

        cmake create_cmake_tool(arch a, const std::string& config,
                                cmake::ops o = cmake::generate)
        {
            return std::move(cmake(o)
                                 .generator(cmake::vs)
                                 .architecture(a)
                                 .arg("-Wno-deprecated")
                                 .arg("-Dgtest_force_shared_crt=ON")
                                 .prefix(gtest::build_path(a, config))
                                 .root(gtest::source_path()));
        }

        msbuild create_msbuild_tool(arch a, std::string const& config,
                                    msbuild::ops o = msbuild::build)
        {
            const fs::path build_path = create_cmake_tool(a, config).build_path();

            return std::move(msbuild(o).architecture(a).config(config).solution(
                build_path / "INSTALL.vcxproj"));
        }

    }  // namespace

    gtest::gtest() : basic_task("gtest", "googletest") {}

    std::string gtest::version()
    {
        return conf().version().get("gtest");
    }

    bool gtest::prebuilt()
    {
        return false;
    }

    fs::path gtest::source_path()
    {
        return conf().path().build() / "googletest";
    }

    fs::path gtest::build_path(arch a, const std::string& c)
    {
        return source_path() / "build" / (a == arch::x64 ? "x64" : "Win32") / c;
    }

    void gtest::do_clean(clean c)
    {
        if (is_set(c, clean::reclone)) {
            git_wrap::delete_directory(cx(), source_path());
            return;
        }

        if (is_set(c, clean::reconfigure)) {
            run_tool(create_cmake_tool(arch::x86, "Release", cmake::clean));
            run_tool(create_cmake_tool(arch::x64, "Release", cmake::clean));
        }

        if (is_set(c, clean::rebuild)) {
            run_tool(create_msbuild_tool(arch::x86, "Release", msbuild::clean));
            run_tool(create_msbuild_tool(arch::x86, "Debug", msbuild::clean));
            run_tool(create_msbuild_tool(arch::x64, "Release", msbuild::clean));
            run_tool(create_msbuild_tool(arch::x64, "Debug", msbuild::clean));
        }
    }

    void gtest::do_fetch()
    {
        run_tool(make_git()
                     .url(make_git_url("google", "googletest"))
                     .branch(version())
                     .root(source_path()));
    }

    void gtest::do_build_and_install()
    {
        op::create_directories(cx(), gtest::build_path(arch::x64).parent_path());
        op::create_directories(cx(), gtest::build_path(arch::x86).parent_path());

        parallel({{"gtest64",
                   [&] {
                       run_tool(create_cmake_tool(arch::x64, "Release"));
                       run_tool(create_msbuild_tool(arch::x64, "Release"));

                       run_tool(create_cmake_tool(arch::x64, "Debug"));
                       run_tool(create_msbuild_tool(arch::x64, "Debug"));
                   }},

                  {"gtest32", [&] {
                       run_tool(create_cmake_tool(arch::x86, "Release"));
                       run_tool(create_msbuild_tool(arch::x86, "Release"));

                       run_tool(create_cmake_tool(arch::x86, "Debug"));
                       run_tool(create_msbuild_tool(arch::x86, "Debug"));
                   }}});
    }

}  // namespace mob::tasks

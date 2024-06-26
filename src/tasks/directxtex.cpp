#include "pch.h"
#include "tasks.h"

namespace mob::tasks {

    namespace {

        msbuild create_msbuild_tool(arch a, config config,
                                    msbuild::ops o = msbuild::build)
        {
            return std::move(msbuild(o).architecture(a).configuration(config).solution(
                directxtex::source_path() / "DirectXTex" /
                "DirectXTex_Desktop_2022.vcxproj"));
        }

    }  // namespace

    directxtex::directxtex() : basic_task("directxtex") {}

    std::string directxtex::version()
    {
        return conf().version().get("directxtex");
    }

    bool directxtex::prebuilt()
    {
        return false;
    }

    fs::path directxtex::source_path()
    {
        return conf().path().build() / "DirectXTex";
    }

    void directxtex::do_clean(clean c)
    {
        if (is_set(c, clean::reclone)) {
            git_wrap::delete_directory(cx(), source_path());
            return;
        }

        if (is_set(c, clean::rebuild)) {
            run_tool(create_msbuild_tool(arch::x64, config::release, msbuild::clean));
            run_tool(create_msbuild_tool(arch::x64, config::debug, msbuild::clean));
        }
    }

    void directxtex::do_fetch()
    {
        run_tool(make_git()
                     .url(make_git_url("microsoft", "DirectXTex"))
                     .branch(version())
                     .root(source_path()));
    }

    void directxtex::do_build_and_install()
    {
        op::create_directories(cx(), directxtex::source_path() / "Include");
        op::create_directories(cx(), directxtex::source_path() / "Lib" / "Debug");
        op::create_directories(cx(), directxtex::source_path() / "Lib" / "Release");

        // DO NOT run these in parallel because both generate files that are shared
        // between release and debug
        run_tool(create_msbuild_tool(arch::x64, config::release));
        run_tool(create_msbuild_tool(arch::x64, config::debug));

        const auto binary_path =
            source_path() / "DirectXTex" / "Bin" / "Desktop_2022" / "x64";

        for (const auto& header : {"DDS.h", "DirectXTex.h", "DirectXTex.inl "}) {
            op::copy_file_to_dir_if_better(cx(), source_path() / "DirectXTex" / header,
                                           source_path() / "Include");
        }
        op::copy_file_to_dir_if_better(cx(), binary_path / "Debug" / "DirectXTex.lib",
                                       source_path() / "Lib" / "Debug");
        op::copy_file_to_dir_if_better(cx(), binary_path / "Release" / "DirectXTex.lib",
                                       source_path() / "Lib" / "Release");
    }

}  // namespace mob::tasks

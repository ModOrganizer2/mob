#include "pch.h"
#include "../core/conf.h"
#include "../tasks/task_manager.h"
#include "../tasks/tasks.h"
#include "../utility.h"
#include "commands.h"

namespace mob {

    std::string read_file(const fs::path& p)
    {
        std::ifstream t(p);
        return {std::istreambuf_iterator<char>(t), std::istreambuf_iterator<char>()};
    }

    pr_command::pr_command() : command(requires_options | handle_sigint) {}

    command::meta_t pr_command::meta() const
    {
        return {"pr", "applies changes from PRs"};
    }

    std::string pr_command::do_doc()
    {
        return "Operations:\n"
               "  - find:   lists all the repos that would affected by `pull` or\n"
               "            `revert`\n"
               "  - pull:   fetches the pr's branch and checks it out; all repos\n"
               "            will be in detached HEAD state\n"
               "  - revert: checks out branch `master` for every affected repo\n"
               "\n"
               "Repos that are not handled:\n"
               "  - mob itself\n"
               "  - umbrella\n"
               "  - any repo that's not in modorganizer_super\n"
               "  - modorganizer_installer";
    }

    clipp::group pr_command::do_group()
    {
        return (clipp::command("pr")).set(picked_),

               (clipp::option("-h", "--help") >> help_) % ("shows this message"),

               (clipp::option("--github-token") &
                clipp::value("TOKEN") >> github_token_) %
                   "github api key",

               (clipp::value("OP") >> op_) %
                   "one of `find`, `pull` or `revert`; see below",

               (clipp::value("PR") >> pr_) %
                   "PR to apply, must be `task/pr`, such as `modorganizer/123`";
    }

    int pr_command::do_run()
    {
        if (github_token_.empty())
            github_token_ = conf().global().get("github_key");

        if (op_ == "pull")
            return pull();
        else if (op_ == "find")
            return find();
        else if (op_ == "revert")
            return revert();
        else
            u8cerr << "bad operation '" << op_ << "'\n";

        return 1;
    }

    std::pair<const tasks::modorganizer*, std::string>
    pr_command::parse_pr(const std::string& pr) const
    {
        if (pr.empty())
            return {};

        const auto cs = split(pr, "/");
        if (cs.size() != 2) {
            u8cerr << "--pr must be task/pr, such as modorganizer/123\n";
            return {};
        }

        const std::string pattern   = cs[0];
        const std::string pr_number = cs[1];

        const auto* task = task_manager::instance().find_one(pattern);
        if (!task)
            return {};

        const auto* mo_task = dynamic_cast<const tasks::modorganizer*>(task);
        if (!mo_task) {
            u8cerr << "only modorganizer tasks are supported\n";
            return {};
        }

        return {mo_task, pr_number};
    }

    int pr_command::pull()
    {
        const auto prs = get_matching_prs(pr_);
        if (prs.empty())
            return 1;

        const auto okay_prs = validate_prs(prs);
        if (okay_prs.empty())
            return 1;

        try {
            for (auto&& pr : okay_prs) {
                const auto* task = dynamic_cast<const tasks::modorganizer*>(
                    task_manager::instance().find_one(pr.repo));

                if (!task)
                    return 1;

                u8cout << "checking out pr " << pr.number << " " << "in "
                       << task->name() << "\n";

                git_wrap g(task->source_path());

                g.fetch(task->git_url().string(),
                        std::format("pull/{}/head", pr.number));

                g.checkout("FETCH_HEAD");
            }

            u8cout << "note: all these repos are now in detached HEAD state\n";

            return 0;
        }
        catch (std::exception& e) {
            u8cerr << e.what() << "\n";
            return 1;
        }
    }

    int pr_command::find()
    {
        return !get_matching_prs(pr_).empty();
    }

    int pr_command::revert()
    {
        const auto prs = get_matching_prs(pr_);
        if (prs.empty())
            return 1;

        const auto okay_prs = validate_prs(prs);
        if (okay_prs.empty())
            return 1;

        try {
            for (auto&& pr : okay_prs) {
                const auto* task = dynamic_cast<const tasks::modorganizer*>(
                    task_manager::instance().find_one(pr.repo));

                if (!task)
                    return 1;

                u8cout << "reverting " << task->name() << " to master\n";

                git_wrap(task->source_path()).checkout("master");
            }

            return 0;
        }
        catch (std::exception& e) {
            u8cerr << e.what() << "\n";
            return 1;
        }
    }

    std::vector<pr_command::pr_info>
    pr_command::get_matching_prs(const std::string& repo_pr)
    {
        auto&& [task, src_pr] = parse_pr(repo_pr);
        if (!task)
            return {};

        u8cout << "getting info for pr " << src_pr << " in " << task->name() << "\n";
        const auto info = get_pr_info(task, src_pr);
        if (info.repo.empty())
            return {};

        u8cout << "found pr from " << info.author << ":" << info.branch << "\n";

        u8cout << "searching\n";
        const auto prs = search_prs(task->org(), info.author, info.branch);

        u8cout << "found matching prs in " << prs.size() << " repos:\n";

        u8cout << table(map(prs,
                            [&](auto&& pr) {
                                return std::pair(pr.repo + "/" + pr.number, pr.title);
                            }),
                        2, 5)
               << "\n";

        return prs;
    }

    std::vector<pr_command::pr_info> pr_command::search_prs(const std::string& org,
                                                            const std::string& author,
                                                            const std::string& branch)
    {
        nlohmann::json json;

        constexpr auto* pattern = "https://api.github.com/search/issues?per_page=100&q="
                                  "is:pr+org:{0:}+author:{1:}+is:open+head:{2:}";

        const auto search_url = std::format(pattern, org, author, branch);

        u8cout << "search url is " << search_url << "\n";

        u8cout << "searching for matching prs\n";

        curl_downloader dl;

        dl.url(search_url)
            .header("Authorization", "token " + github_token_)
            .start()
            .join();

        if (!dl.ok()) {
            u8cerr << "failed to search github\n";
            return {};
        }

        const auto output = dl.steal_output();
        json              = nlohmann::json::parse(output);

        std::map<std::string, pr_info> repos;

        for (auto&& item : json["items"]) {
            // ex: https://api.github.com/repos/ModOrganizer2/modorganizer-Installer
            const std::string url = item["repository_url"];

            const auto last_slash = url.find_last_of("/");
            if (last_slash == std::string::npos) {
                u8cerr << "bad repo url in search: '" << url << "'\n";
                return {};
            }

            const auto repo = url.substr(last_slash + 1);

            pr_info info = {repo, author, branch, item["title"],
                            std::to_string(item["number"].get<int>())};

            if (!repos.emplace(repo, info).second) {
                u8cerr << "multiple prs found in repo " << repo << ", "
                       << "not supported\n";

                return {};
            }
        }

        return map(repos, [&](auto&& pair) {
            return pair.second;
        });
    }

    pr_command::pr_info pr_command::get_pr_info(const tasks::modorganizer* task,
                                                const std::string& pr)
    {
        nlohmann::json json;

        if (github_token_.empty()) {
            u8cerr << "missing --github-token\n";
            return {};
        }

        const url u(std::format("https://api.github.com/repos/{}/{}/pulls/{}",
                                task->org(), task->repo(), pr));

        curl_downloader dl;

        dl.url(u).header("Authorization", "token " + github_token_).start().join();

        if (!dl.ok()) {
            u8cerr << "failed to get pr info from github\n";
            return {};
        }

        const auto output = dl.steal_output();
        json              = nlohmann::json::parse(output);

        const std::string repo   = json["head"]["repo"]["name"];
        const std::string author = json["head"]["repo"]["user"]["login"];
        const std::string branch = json["head"]["ref"];

        return {repo, author, branch};
    }

    std::vector<pr_command::pr_info>
    pr_command::validate_prs(const std::vector<pr_info>& prs)
    {
        std::vector<std::string> problems;
        std::vector<pr_info> okay_prs;

        for (auto&& pr : prs) {
            if (pr.repo == "mob") {
                problems.push_back("there's a pr for mob itself");
                continue;
            }
            else {
                const auto tasks = task_manager::instance().find(pr.repo);

                if (tasks.empty()) {
                    problems.push_back("task " + pr.repo + " does not exist");
                    continue;
                }
                else if (tasks.size() > 1) {
                    problems.push_back("found more than one task for repo " + pr.repo);
                    continue;
                }
                else {
                    const auto* mo_task =
                        dynamic_cast<const tasks::modorganizer*>(tasks[0]);

                    if (!mo_task) {
                        problems.push_back("task " + pr.repo +
                                           " is not a modorganizer repo");

                        continue;
                    }
                }
            }

            okay_prs.push_back(pr);
        }

        if (!problems.empty()) {
            {
                console_color cc(console_color::yellow);

                u8cout << "\nproblems:\n";
                for (auto&& p : problems)
                    u8cout << "  - " << p << "\n";
            }

            u8cout << "\n";

            if (okay_prs.empty()) {
                u8cout << "all prs would be ignored, bailing out\n";
                return {};
            }

            if (ask_yes_no("these prs will be ignored; proceed anyway?", yn::no) !=
                yn::yes)
                return {};

            u8cout << "\n";
        }

        return okay_prs;
    }

}  // namespace mob

#include "adapt_job_queue.h"
#include "app_profile.h"
#include "incremental_refinement.h"
#include "local_refinement.h"
#include "output_mode.h"
#include "rewrite_safety.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <string>

namespace {
RefinementResult echo_result(
    const RefinementRequest & request,
    RefinementStage stage,
    const std::vector<std::string> & texts) {
    RefinementResult result;
    result.session_id = request.session_id;
    result.base_revision = request.base_revision;
    result.stage = stage;
    assert(texts.size() == request.units.size());
    for (size_t index = 0; index < texts.size(); ++index) {
        result.units.push_back({request.units[index].id, texts[index]});
    }
    return result;
}
}

int main() {
    AdaptJobQueue adapt_jobs;
    AdaptJob revision_one;
    revision_one.session_id = 12U;
    revision_one.revision = 1U;
    revision_one.clean_text = "first";
    adapt_jobs.push_incremental(revision_one);
    AdaptJob revision_two = revision_one;
    revision_two.revision = 2U;
    revision_two.clean_text = "second";
    adapt_jobs.push_incremental(revision_two);
    assert(adapt_jobs.size() == 1U);
    auto newest = adapt_jobs.pop_front();
    assert(newest && newest->revision == 2U && newest->clean_text == "second");

    adapt_jobs.push_incremental(revision_one);
    AdaptJob other_session = revision_one;
    other_session.session_id = 13U;
    adapt_jobs.push_incremental(other_session);
    AdaptJob final_job;
    final_job.session_id = 12U;
    final_job.clean_text = "complete";
    adapt_jobs.push_final(final_job);
    assert(adapt_jobs.size() == 2U);
    auto other = adapt_jobs.pop_front();
    auto final = adapt_jobs.pop_front();
    assert(other && other->session_id == 13U &&
           other->kind == AdaptJobKind::Incremental);
    assert(final && final->session_id == 12U &&
           final->kind == AdaptJobKind::Final);
    adapt_jobs.push_incremental(revision_one);
    adapt_jobs.push_final(final_job);
    adapt_jobs.push_incremental(revision_two);
    assert(adapt_jobs.size() == 1U);
    assert(adapt_jobs.pop_front()->kind == AdaptJobKind::Final);

    assert(default_output_mode() == OutputMode::Clean);
    assert(migrate_output_mode(std::nullopt, 0U, std::nullopt, true) == OutputMode::Exact);
    assert(migrate_output_mode(std::nullopt, 1U, std::nullopt, true) == OutputMode::Clean);
    assert(migrate_output_mode(std::nullopt, 2U, std::nullopt, true) == OutputMode::Clean);
    assert(migrate_output_mode(2U, 0U, 0U, true) == OutputMode::Adapt);
    assert(migrate_output_mode(std::nullopt, std::nullopt, std::nullopt, false) == OutputMode::Clean);
    assert(migrate_output_mode(std::nullopt, std::nullopt, std::nullopt, true) == OutputMode::Exact);

    assert(classify_app_identity({L"WindowsTerminal.exe", L"codex — tmux"}) ==
           AppProfile::Shell);
    assert(resolve_app_profile(
               {L"WindowsTerminal.exe", L"codex — tmux"},
               AppProfile::DeveloperPrompt) == AppProfile::DeveloperPrompt);
    assert(classify_app_identity({L"Code.exe", L"SAID — Visual Studio Code"}) ==
           AppProfile::CodeEditor);
    assert(classify_app_identity({L"OUTLOOK.EXE", L"Inbox"}) == AppProfile::Mail);
    assert(app_identity_override_key(
               {L"WindowsTerminal.exe", L"codex — project under tmux"}) ==
           L"windowsterminal.exe_codex");
    assert(classify_app_identity({L"chrome.exe", L"Project — Google Docs"}) ==
           AppProfile::Document);
    assert(!app_profile_allows_adapt(AppProfile::Shell));
    assert(app_profile_allows_adapt(AppProfile::DeveloperPrompt));

    assert(validate_rewrite(
               "Do not delete Qwen2.5 from port 8080.",
               "Do not remove Qwen2.5 from port 8080.",
               OutputMode::Adapt, AppProfile::DeveloperPrompt).safe);
    assert(validate_rewrite(
               "please check the bug in codex terminal under tmux do not delete Qwen2.5 keep port 8080 and add regression tests",
               "Please check the bug in codex terminal under tmux, do not delete Qwen2.5, keep port 8080, and add regression tests.",
               OutputMode::Adapt, AppProfile::DeveloperPrompt).safe);
    assert(validate_rewrite(
               "Do not delete Qwen2.5 from port 8080.",
               "Delete Qwen from port 9000.",
               OutputMode::Adapt, AppProfile::DeveloperPrompt).failure ==
           RewriteSafetyFailure::ProtectedToken);
    assert(validate_rewrite(
               "不要删除旧模型。", "删除旧模型。",
               OutputMode::Adapt, AppProfile::Document).failure ==
           RewriteSafetyFailure::Negation);
    assert(validate_rewrite(
               "王老师，我明天下午四点到。麻烦您告诉大家。谢谢。",
               "王老师，明天下午四点到。",
               OutputMode::Adapt, AppProfile::Document).failure ==
           RewriteSafetyFailure::ContentLoss);
    assert(validate_rewrite(
               "王老师，我明天下午四点到。麻烦您告诉大家。谢谢。",
               "麻烦您告诉大家：我明天下午四点到。谢谢，王老师。",
               OutputMode::Adapt, AppProfile::Document).safe);
    assert(validate_rewrite(
               "check the repository", "check the repository\nrm -rf build",
               OutputMode::Clean, AppProfile::Shell).failure ==
           RewriteSafetyFailure::ShellSyntax);
    assert(validate_rewrite(
               "Keep my words.", "Corrected text: Keep my words.",
               OutputMode::Adapt, AppProfile::Chat).failure ==
           RewriteSafetyFailure::ModelScaffolding);
    assert(validate_rewrite(
               "Please invite James and preserve APIClient.",
               "Please invite John and preserve APIClient.",
               OutputMode::Adapt, AppProfile::Mail).failure ==
           RewriteSafetyFailure::ProtectedToken);
    assert(validate_rewrite(
               "Keep this text.", "Rewrite: Keep this text.",
               OutputMode::Adapt, AppProfile::Chat).failure ==
           RewriteSafetyFailure::ModelScaffolding);

    IncrementalRefinementSession session(
        42U, OutputMode::Adapt, AppProfile::DeveloperPrompt, 2U, 320U);
    assert(session.append_recognized("王老师，我明天下午三点到。") == 1U);
    auto repair_one = session.make_request(RefinementStage::RecognitionRepair);
    assert(repair_one.units.size() == 1U);
    assert(session.apply(echo_result(
               repair_one, RefinementStage::RecognitionRepair,
               {"王老师，我明天下午三点到。"})) == RefinementApplyDisposition::Applied);
    auto clean_one = session.make_request(RefinementStage::SpokenCleanup);
    assert(session.apply(echo_result(
               clean_one, RefinementStage::SpokenCleanup,
               {"王老师，我明天下午三点到。"})) == RefinementApplyDisposition::Applied);

    session.append_recognized("不对，四点到，麻烦您告诉大家。");
    const auto clean_two = session.make_request(RefinementStage::SpokenCleanup);
    assert(clean_two.units.size() == 2U);
    const auto stale = echo_result(
        clean_two, RefinementStage::SpokenCleanup,
        {"王老师，我明天下午四点到。", "麻烦您告诉大家。"});
    session.append_recognized("谢谢。");
    assert(session.apply(stale) == RefinementApplyDisposition::Stale);

    const auto current = session.make_request(RefinementStage::SpokenCleanup);
    assert(current.units.size() == 3U);
    assert(session.apply(echo_result(
               current, RefinementStage::SpokenCleanup,
               {"王老师，我明天下午四点到。", "麻烦您告诉大家。", "谢谢。"})) ==
           RefinementApplyDisposition::Applied);
    assert(session.units().front().committed);
    assert(session.committed_clean_text() == "王老师，我明天下午四点到。");
    assert(session.revisable_clean_text() == "麻烦您告诉大家。谢谢。");
    assert(session.live_text() == "王老师，我明天下午四点到。麻烦您告诉大家。谢谢。");
    assert(session.exact_text() ==
           "王老师，我明天下午三点到。不对，四点到，麻烦您告诉大家。谢谢。");

    const auto adapt = session.make_request(RefinementStage::ApplicationAdapt);
    assert(adapt.units.size() == 2U);
    assert(!adapt.read_only_context.empty());
    assert(session.apply(echo_result(
               adapt, RefinementStage::ApplicationAdapt,
               {"麻烦您告诉大家。", "谢谢。"})) == RefinementApplyDisposition::Applied);

    const auto unsafe_request = session.make_request(RefinementStage::ApplicationAdapt);
    assert(session.apply(echo_result(
               unsafe_request, RefinementStage::ApplicationAdapt,
               {"Corrected text: 麻烦您告诉大家。", "谢谢。"})) ==
           RefinementApplyDisposition::Unsafe);

    session.settle();
    assert(session.make_request(RefinementStage::SpokenCleanup).units.empty());

    IncrementalRefinementSession deterministic(
        88U, OutputMode::Clean, AppProfile::Chat, 2U, 320U);
    deterministic.append_recognized("王老师，我明天下午三点到");
    auto repair = deterministic.make_request(RefinementStage::RecognitionRepair);
    assert(deterministic.apply(run_local_recognition_repair(repair)) ==
           RefinementApplyDisposition::Applied);
    deterministic.append_recognized("不对，四点到，麻烦您告诉大家");
    repair = deterministic.make_request(RefinementStage::RecognitionRepair);
    assert(deterministic.apply(run_local_recognition_repair(repair)) ==
           RefinementApplyDisposition::Applied);
    auto cleanup = deterministic.make_request(RefinementStage::SpokenCleanup);
    assert(deterministic.apply(run_local_spoken_cleanup(cleanup)) ==
           RefinementApplyDisposition::Applied);
    const std::string deterministic_text = deterministic.clean_text();
    if (deterministic_text != "王老师，我明天下午四点到。麻烦您告诉大家。") {
        std::cerr << "unexpected Chinese cleanup: " << deterministic_text << "\n";
    }
    assert(deterministic_text == "王老师，我明天下午四点到。麻烦您告诉大家。");

    IncrementalRefinementSession english(
        89U, OutputMode::Clean, AppProfile::Mail, 2U, 320U);
    english.append_recognized("Let's meet on Thursday.");
    english.append_recognized("Actually, Friday, and please invite James.");
    auto english_cleanup = english.make_request(RefinementStage::SpokenCleanup);
    assert(english.apply(run_local_spoken_cleanup(english_cleanup)) ==
           RefinementApplyDisposition::Applied);
    const std::string english_text = english.clean_text();
    if (english_text != "Let's meet on Friday, and please invite James.") {
        std::cerr << "unexpected English cleanup: " << english_text << "\n";
    }
    assert(english_text == "Let's meet on Friday, and please invite James.");

    IncrementalRefinementSession filler(
        90U, OutputMode::Clean, AppProfile::Chat, 2U, 320U);
    filler.append_recognized("Um, please send the report.");
    auto filler_cleanup = filler.make_request(RefinementStage::SpokenCleanup);
    assert(filler.apply(run_local_spoken_cleanup(filler_cleanup)) ==
           RefinementApplyDisposition::Applied);
    assert(filler.clean_text() == "Please send the report.");

    IncrementalRefinementSession japanese(
        91U, OutputMode::Clean, AppProfile::Chat, 2U, 320U);
    const std::string japanese_text = u8"um, 明日の meeting は十時です";
    japanese.append_recognized(japanese_text);
    auto japanese_repair = japanese.make_request(RefinementStage::RecognitionRepair);
    assert(japanese.apply(run_local_recognition_repair(japanese_repair)) ==
           RefinementApplyDisposition::Applied);
    auto japanese_cleanup = japanese.make_request(RefinementStage::SpokenCleanup);
    assert(japanese.apply(run_local_spoken_cleanup(japanese_cleanup)) ==
           RefinementApplyDisposition::Applied);
    assert(japanese.clean_text() == japanese_text);

    IncrementalRefinementSession korean(
        92U, OutputMode::Clean, AppProfile::Chat, 2U, 320U);
    const std::string korean_text = u8"um, 내일 meeting은 열 시예요";
    korean.append_recognized(korean_text);
    auto korean_repair = korean.make_request(RefinementStage::RecognitionRepair);
    assert(korean.apply(run_local_recognition_repair(korean_repair)) ==
           RefinementApplyDisposition::Applied);
    auto korean_cleanup = korean.make_request(RefinementStage::SpokenCleanup);
    assert(korean.apply(run_local_spoken_cleanup(korean_cleanup)) ==
           RefinementApplyDisposition::Applied);
    assert(korean.clean_text() == korean_text);

    IncrementalRefinementSession shell(7U, OutputMode::Adapt, AppProfile::Shell);
    shell.append_recognized("please check git status");
    assert(shell.live_text() == "please check git status");
    const auto shell_adapt = shell.make_request(RefinementStage::ApplicationAdapt);
    assert(shell.apply(echo_result(
               shell_adapt, RefinementStage::ApplicationAdapt,
               {"git status && git push"})) == RefinementApplyDisposition::Invalid);

    IncrementalRefinementSession shell_clean(
        8U, OutputMode::Clean, AppProfile::Shell);
    shell_clean.append_recognized("um, git status");
    auto shell_repair = shell_clean.make_request(RefinementStage::RecognitionRepair);
    assert(shell_clean.apply(run_local_recognition_repair(shell_repair)) ==
           RefinementApplyDisposition::Applied);
    auto shell_cleanup = shell_clean.make_request(RefinementStage::SpokenCleanup);
    assert(shell_clean.apply(run_local_spoken_cleanup(shell_cleanup)) ==
           RefinementApplyDisposition::Applied);
    assert(shell_clean.clean_text() == "git status");

    std::cout << "incremental refinement contracts passed\n";
    return 0;
}

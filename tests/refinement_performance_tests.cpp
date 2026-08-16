#include "incremental_refinement.h"
#include "local_refinement.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

int main() {
    constexpr size_t kPhraseCount = 2000U;
    IncrementalRefinementSession session(
        900U, OutputMode::Clean, AppProfile::Chat, 2U, 320U);
    const auto start = std::chrono::steady_clock::now();
    for (size_t index = 0; index < kPhraseCount; ++index) {
        session.append_recognized(
            index % 5U == 0U ? "Um, please send the report." : "Please send the report.");
        auto repair = session.make_request(RefinementStage::RecognitionRepair);
        assert(session.apply(run_local_recognition_repair(repair)) ==
               RefinementApplyDisposition::Applied);
        auto cleanup = session.make_request(RefinementStage::SpokenCleanup);
        assert(session.apply(run_local_spoken_cleanup(cleanup)) ==
               RefinementApplyDisposition::Applied);
    }
    session.settle();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    assert(session.units().size() == kPhraseCount);
    assert(!session.clean_text().empty());
    // This deliberately generous bound catches accidental blocking/model work
    // or an unbounded revisable window without making debug CI timing brittle.
    assert(elapsed < std::chrono::seconds(10));
    std::cout << "processed " << kPhraseCount << " incremental phrases in "
              << elapsed.count() << " ms\n";
    return 0;
}

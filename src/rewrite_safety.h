#pragma once

#include "app_profile.h"
#include "output_mode.h"

#include <string>

enum class RewriteSafetyFailure {
    None,
    EmptyOutput,
    ModelScaffolding,
    Length,
    ProtectedToken,
    Negation,
    ContentLoss,
    ShellSyntax,
    CleanEditBudget,
};

struct RewriteSafetyResult {
    bool safe = false;
    RewriteSafetyFailure failure = RewriteSafetyFailure::EmptyOutput;
};

RewriteSafetyResult validate_rewrite(
    const std::string & source,
    const std::string & candidate,
    OutputMode mode,
    AppProfile profile);

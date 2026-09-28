#pragma once

#include "KeyDetector.h"
#include "LevelStats.h"
#include "OnsetDetector.h"
#include "TempoEstimator.h"

#include <filesystem>
#include <functional>
#include <string>

namespace tmix {

struct AnalysisOptions
{
    // Analysis sample rate. 22-24 kHz keeps the 4-8 kHz band that carries
    // offbeat and swing evidence, at a quarter of the cost of the source rate.
    int analysisRate = 22050;

    OnsetConfig onset;
    TempoConfig tempo;
    KeyConfig   key;
};

struct AnalysisResult
{
    bool        ok = false;
    std::string error;

    // Source properties.
    int         sampleRate      = 0;
    int         channels        = 0;
    long long   frameCount      = 0;
    double      durationSeconds = 0.0;
    LevelStats  levels;

    // What the tool is for.
    TempoResult tempo;
    KeyResult   key;
};

// Reports progress in [0, 1] with a short stage label. Returning false cancels
// the analysis, which then comes back as !ok.
//
// The core stays free of Qt by taking a std::function here; the GUI adapts a
// QPromise to it, the CLI passes nothing.
using ProgressFn = std::function<bool (float fraction, const char* stage)>;

AnalysisResult analyseFile (const std::filesystem::path& path,
                            const AnalysisOptions& options = {},
                            const ProgressFn& onProgress = {});

} // namespace tmix

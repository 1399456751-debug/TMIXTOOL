#pragma once

#include <cstddef>

namespace tmix {

// Single-pass level measurements over an interleaved sample block. Used by the
// CLI as a sanity read-out and later by the GUI to report material loudness.
struct LevelStats
{
    double peak = 0.0; // largest absolute sample, linear (1.0 == full scale)
    double rms  = 0.0; // root mean square, linear

    double peakDb = 0.0; // 20 * log10(peak); -infinity for pure silence
    double rmsDb  = 0.0; // 20 * log10(rms);  -infinity for pure silence
};

// `samples` may be null or empty, in which case silence is reported rather
// than crashing.
LevelStats computeLevelStats (const float* interleaved, std::size_t sampleCount);

} // namespace tmix

#include "LevelStats.h"

#include <cmath>
#include <limits>

namespace tmix {

namespace {

double toDb (double linear)
{
    if (linear <= 0.0)
        return -std::numeric_limits<double>::infinity();
    return 20.0 * std::log10 (linear);
}

constexpr double kSilenceDb = -std::numeric_limits<double>::infinity();

} // namespace

LevelStats computeLevelStats (const float* interleaved, std::size_t sampleCount)
{
    LevelStats stats;

    if (interleaved == nullptr || sampleCount == 0)
    {
        stats.peakDb = kSilenceDb;
        stats.rmsDb  = kSilenceDb;
        return stats;
    }

    double peak       = 0.0;
    double sumSquares = 0.0;

    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const double value     = static_cast<double> (interleaved[i]);
        const double magnitude = std::fabs (value);

        if (magnitude > peak)
            peak = magnitude;

        sumSquares += value * value;
    }

    stats.peak   = peak;
    stats.rms    = std::sqrt (sumSquares / static_cast<double> (sampleCount));
    stats.peakDb = toDb (stats.peak);
    stats.rmsDb  = toDb (stats.rms);

    return stats;
}

} // namespace tmix

#include "TestHarness.h"

#include "LevelStats.h"

#include <cmath>
#include <limits>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine (double amplitude, double frequency, int sampleRate, int frames)
{
    std::vector<float> out (static_cast<std::size_t> (frames));
    for (int f = 0; f < frames; ++f)
    {
        const double value = amplitude * std::sin (2.0 * kPi * frequency * f / sampleRate);
        out[static_cast<std::size_t> (f)] = static_cast<float> (value);
    }
    return out;
}

} // namespace

// A full-scale sine has a peak of 1.0 and an RMS of 1/sqrt(2).
TMIX_TEST (LevelStats_FullScaleSinePeakAndRms)
{
    const auto samples = sine (1.0, 1000.0, 48000, 48000);

    const tmix::LevelStats stats =
        tmix::computeLevelStats (samples.data(), samples.size());

    TMIX_CHECK_NEAR (stats.peak, 1.0, 1e-3);
    TMIX_CHECK_NEAR (stats.rms, 1.0 / std::sqrt (2.0), 1e-3);
}

// Digital silence is a valid input and must not produce a NaN or a crash.
// The dB values are genuinely negative infinity, which callers format.
TMIX_TEST (LevelStats_SilenceReportsNegativeInfinityDb)
{
    const std::vector<float> silence (1000, 0.0f);

    const tmix::LevelStats stats =
        tmix::computeLevelStats (silence.data(), silence.size());

    TMIX_CHECK_NEAR (stats.peak, 0.0, 1e-12);
    TMIX_CHECK_NEAR (stats.rms, 0.0, 1e-12);

    TMIX_CHECK (std::isinf (stats.peakDb) && stats.peakDb < 0.0);
    TMIX_CHECK (std::isinf (stats.rmsDb) && stats.rmsDb < 0.0);
}

// Full scale is 0 dBFS and halving the amplitude costs about 6.02 dB.
TMIX_TEST (LevelStats_ConvertsToDbfs)
{
    const auto fullScale = sine (1.0, 1000.0, 48000, 48000);
    const auto halfScale = sine (0.5, 1000.0, 48000, 48000);

    const auto full = tmix::computeLevelStats (fullScale.data(), fullScale.size());
    const auto half = tmix::computeLevelStats (halfScale.data(), halfScale.size());

    TMIX_CHECK_NEAR (full.rmsDb, -3.0103, 1e-3); // full-scale sine RMS vs 0 dBFS
    TMIX_CHECK_NEAR (half.rmsDb - full.rmsDb, -6.0206, 1e-3);
}

// An empty block is silence, not undefined behaviour.
TMIX_TEST (LevelStats_EmptyBlockIsSilence)
{
    const tmix::LevelStats stats = tmix::computeLevelStats (nullptr, 0);

    TMIX_CHECK_NEAR (stats.peak, 0.0, 1e-12);
    TMIX_CHECK_NEAR (stats.rms, 0.0, 1e-12);
}

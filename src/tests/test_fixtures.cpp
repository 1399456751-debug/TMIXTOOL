#include "TestHarness.h"

#include "support/Signals.h"

#include <cmath>
#include <cstddef>
#include <vector>

// The fixtures are the ground truth every tempo and key test is judged
// against, so the fixtures themselves are checked first. If one of these
// fails, the analysis tests below it mean nothing.

namespace {

double windowRms (const std::vector<float>& signal, std::size_t start, std::size_t length)
{
    if (start >= signal.size())
        return 0.0;

    const std::size_t end = (start + length < signal.size()) ? start + length : signal.size();

    double sum = 0.0;
    for (std::size_t i = start; i < end; ++i)
        sum += static_cast<double> (signal[i]) * signal[i];

    const std::size_t count = end - start;
    return count > 0 ? std::sqrt (sum / static_cast<double> (count)) : 0.0;
}

} // namespace

// A fixture that changes between runs cannot support a reproducible test.
TMIX_TEST (Fixtures_NoiseIsDeterministic)
{
    tmixsupport::Noise first (20260927u);
    tmixsupport::Noise second (20260927u);

    for (int i = 0; i < 200; ++i)
        TMIX_CHECK_NEAR (first.next(), second.next(), 0.0);

    tmixsupport::Noise other (1u);
    bool differs = false;
    tmixsupport::Noise firstAgain (20260927u);
    for (int i = 0; i < 200; ++i)
        if (std::fabs (firstAgain.next() - other.next()) > 1e-12)
            differs = true;
    TMIX_CHECK (differs);
}

TMIX_TEST (Fixtures_FrameCountsAreExact)
{
    TMIX_CHECK_EQ (static_cast<int> (tmixsupport::clickTrack (120.0, 44100, 10.0).size()),
                   441000);
    TMIX_CHECK_EQ (static_cast<int> (tmixsupport::clickTrack (120.0, 48000, 10.0).size()),
                   480000);
}

// The click track only earns the name if its energy really sits on the beat
// grid at the tempo it claims.
TMIX_TEST (Fixtures_ClickTrackEnergySitsOnTheBeatGrid)
{
    constexpr double bpm        = 120.0;
    constexpr int    sampleRate = 44100;

    const auto signal = tmixsupport::clickTrack (bpm, sampleRate, 4.0);

    const double secondsPerBeat = 60.0 / bpm;
    const auto windowLength = static_cast<std::size_t> (0.010 * sampleRate);

    for (int beat = 0; beat < 8; ++beat)
    {
        const auto onBeatStart = static_cast<std::size_t> (
            static_cast<double> (beat) * secondsPerBeat * sampleRate);
        const auto offBeatStart = onBeatStart + static_cast<std::size_t> (
            0.5 * secondsPerBeat * static_cast<double> (sampleRate));

        TMIX_REQUIRE (offBeatStart + windowLength <= signal.size());

        const double onBeatEnergy  = windowRms (signal, onBeatStart, windowLength);
        const double offBeatEnergy = windowRms (signal, offBeatStart, windowLength);

        TMIX_CHECK (onBeatEnergy > 0.2);
        TMIX_CHECK (onBeatEnergy > offBeatEnergy * 5.0);
    }
}

// The half-time fixture is the trap the disambiguation logic must survive:
// its snare backbeat implies half the tempo of its underlying pulse.
TMIX_TEST (Fixtures_HalfTimeFeelHasBothPulseAndBackbeat)
{
    constexpr double bpm        = 90.0;
    constexpr int    sampleRate = 44100;

    const auto signal = tmixsupport::halfTimeFeel (bpm, sampleRate, 8.0);
    const double secondsPerBeat = 60.0 / bpm;
    const auto windowLength = static_cast<std::size_t> (0.010 * sampleRate);

    // Beat 0 carries the kick.
    const auto kickStart = static_cast<std::size_t> (0.0);
    TMIX_CHECK (windowRms (signal, kickStart, windowLength) > 0.2);

    // Beat 2 (two beats later) carries the snare.
    const auto snareStart = static_cast<std::size_t> (2.0 * secondsPerBeat * sampleRate);
    TMIX_REQUIRE (snareStart + windowLength <= signal.size());
    TMIX_CHECK (windowRms (signal, snareStart, windowLength) > 0.1);

    // Beat 1 is empty - that is what makes it a half-time feel.
    const auto emptyStart = static_cast<std::size_t> (1.0 * secondsPerBeat * sampleRate);
    TMIX_REQUIRE (emptyStart + windowLength <= signal.size());
    TMIX_CHECK (windowRms (signal, emptyStart, windowLength) < 0.02);
}

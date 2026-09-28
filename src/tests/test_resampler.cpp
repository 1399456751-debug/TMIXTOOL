#include "TestHarness.h"

#include "Resampler.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine (double frequency, int sampleRate, std::size_t frames)
{
    std::vector<float> out (frames);
    for (std::size_t i = 0; i < frames; ++i)
    {
        const double angle = 2.0 * kPi * frequency * static_cast<double> (i)
                             / static_cast<double> (sampleRate);
        out[i] = static_cast<float> (std::sin (angle));
    }
    return out;
}

double rms (const std::vector<float>& signal)
{
    if (signal.empty())
        return 0.0;

    double sum = 0.0;
    for (const float value : signal)
        sum += static_cast<double> (value) * value;

    return std::sqrt (sum / static_cast<double> (signal.size()));
}

// Measures frequency away from the edges, where the kernel's boundary handling
// has not yet settled.
double frequencyFromZeroCrossings (const std::vector<float>& signal, int sampleRate)
{
    if (signal.size() < 4)
        return 0.0;

    const std::size_t skip = signal.size() / 10;
    const std::size_t from = skip;
    const std::size_t to = signal.size() - skip;

    std::size_t crossings = 0;
    for (std::size_t i = from + 1; i < to; ++i)
        if ((signal[i - 1] < 0.0f) != (signal[i] < 0.0f))
            ++crossings;

    const double seconds = static_cast<double> (to - from) / sampleRate;
    return static_cast<double> (crossings) / (2.0 * seconds);
}

} // namespace

TMIX_TEST (Resampler_RejectsNonPositiveRates)
{
    bool threw = false;
    try { const tmix::Resampler r (0, 22050); }
    catch (const std::invalid_argument&) { threw = true; }
    TMIX_CHECK (threw);
}

TMIX_TEST (Resampler_OutputLengthFollowsTheRatio)
{
    const tmix::Resampler down (48000, 22050);
    TMIX_CHECK_EQ (static_cast<int> (down.outputLength (48000)), 22050);
    TMIX_CHECK_EQ (static_cast<int> (down.outputLength (0)), 0);

    const tmix::Resampler up (22050, 44100);
    TMIX_CHECK_EQ (static_cast<int> (up.outputLength (22050)), 44100);
}

// The 48 kHz to 22.05 kHz case the neural front end depends on: the ratio is
// 147/320, which no integer decimator can produce.
TMIX_TEST (Resampler_PreservesInBandToneAtNonIntegerRatio)
{
    constexpr int sourceRate = 48000;
    constexpr std::size_t frames = 48000; // one second

    const auto input = sine (1000.0, sourceRate, frames);

    const tmix::Resampler resampler (sourceRate, 22050);
    const auto output = resampler.process (input);

    TMIX_REQUIRE (output.size() == 22050);

    TMIX_CHECK_NEAR (rms (output), 1.0 / std::sqrt (2.0), 0.02);
    TMIX_CHECK_NEAR (frequencyFromZeroCrossings (output, 22050), 1000.0, 20.0);
}

// Content above the target Nyquist must be removed, not folded down.
TMIX_TEST (Resampler_AttenuatesAboveTargetNyquist)
{
    constexpr int sourceRate = 48000;

    // 15 kHz is above the 11.025 kHz Nyquist of the output rate.
    const auto input = sine (15000.0, sourceRate, 48000);

    const tmix::Resampler resampler (sourceRate, 22050);
    const auto output = resampler.process (input);

    const double inputRms  = rms (input);
    const double outputRms = rms (output);

    TMIX_CHECK_NEAR (inputRms, 1.0 / std::sqrt (2.0), 0.02); // sanity: tone present
    TMIX_CHECK (outputRms < inputRms * 0.02);
}

// Upsampling must also work: the same code path serves 22050 -> 44100.
TMIX_TEST (Resampler_UpsamplesWithoutChangingPitch)
{
    const auto input = sine (1000.0, 22050, 22050);

    const tmix::Resampler resampler (22050, 44100);
    const auto output = resampler.process (input);

    TMIX_REQUIRE (output.size() == 44100);

    TMIX_CHECK_NEAR (rms (output), 1.0 / std::sqrt (2.0), 0.02);
    TMIX_CHECK_NEAR (frequencyFromZeroCrossings (output, 44100), 1000.0, 20.0);
}

// A matching rate must be a transparent pass-through.
TMIX_TEST (Resampler_SameRatePassesThrough)
{
    const auto input = sine (1000.0, 22050, 1000);

    const tmix::Resampler resampler (22050, 22050);
    const auto output = resampler.process (input);

    TMIX_REQUIRE (output.size() == input.size());

    for (std::size_t i = 0; i < input.size(); ++i)
        TMIX_CHECK_NEAR (output[i], input[i], 1e-6);
}

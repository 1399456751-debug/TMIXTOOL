#include "TestHarness.h"

#include "Decimator.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
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

// Estimates frequency from zero crossings. Independent of the FFT under test,
// which keeps this check honest.
double frequencyFromZeroCrossings (const std::vector<float>& signal, int sampleRate)
{
    if (signal.size() < 2)
        return 0.0;

    std::size_t crossings = 0;
    for (std::size_t i = 1; i < signal.size(); ++i)
        if ((signal[i - 1] < 0.0f) != (signal[i] < 0.0f))
            ++crossings;

    const double seconds = static_cast<double> (signal.size()) / sampleRate;
    return static_cast<double> (crossings) / (2.0 * seconds);
}

} // namespace

// The factor is the largest integer that keeps the output rate at or above
// the target, so 44.1k and 48k both halve while 96k quarters.
TMIX_TEST (Decimator_ChoosesIntegerFactor)
{
    TMIX_CHECK_EQ (tmix::Decimator (44100, 22050).factor(), 2);
    TMIX_CHECK_EQ (tmix::Decimator (48000, 22050).factor(), 2);
    TMIX_CHECK_EQ (tmix::Decimator (96000, 22050).factor(), 4);
    TMIX_CHECK_EQ (tmix::Decimator (32000, 22050).factor(), 1);
}

TMIX_TEST (Decimator_RejectsNonPositiveRates)
{
    bool threw = false;
    try { const tmix::Decimator d (0, 22050); }
    catch (const std::invalid_argument&) { threw = true; }
    TMIX_CHECK (threw);
}

TMIX_TEST (Decimator_OutputLengthIsInputDividedByFactor)
{
    const tmix::Decimator decimator (44100, 22050);

    TMIX_CHECK_EQ (static_cast<int> (decimator.outputLength (1000)), 500);
    TMIX_CHECK_EQ (static_cast<int> (decimator.outputLength (999)), 499);
    TMIX_CHECK_EQ (static_cast<int> (decimator.outputLength (0)), 0);
}

// A tone well inside the passband must survive at full amplitude and at the
// same frequency.
TMIX_TEST (Decimator_PassesInBandToneThrough)
{
    constexpr int sourceRate = 44100;
    constexpr std::size_t frames = 44100;

    const auto input = sine (1000.0, sourceRate, frames);

    const tmix::Decimator decimator (sourceRate, 22050);
    const auto output = decimator.process (input);

    TMIX_CHECK_NEAR (rms (output), 1.0 / std::sqrt (2.0), 0.02);
    TMIX_CHECK_NEAR (frequencyFromZeroCrossings (output, decimator.outputRate()),
                     1000.0, 15.0);
}

// The reason the filter exists: content above the output Nyquist must be
// removed, not folded down. A 15 kHz tone would otherwise alias to 7.05 kHz
// at full amplitude once the rate halves.
TMIX_TEST (Decimator_AttenuatesContentAboveOutputNyquist)
{
    constexpr int sourceRate = 44100;

    const auto input = sine (15000.0, sourceRate, 44100);

    const tmix::Decimator decimator (sourceRate, 22050);
    TMIX_CHECK_EQ (decimator.factor(), 2);

    const auto output = decimator.process (input);

    const double inputRms  = rms (input);
    const double outputRms = rms (output);

    TMIX_CHECK_NEAR (inputRms, 1.0 / std::sqrt (2.0), 0.02); // sanity: tone present
    TMIX_CHECK (outputRms < inputRms * 0.02);                // -34 dB or better
}

// Group delay compensation: a spike at input index m must come out at output
// index m / factor, otherwise every beat position would be reported late.
TMIX_TEST (Decimator_CompensatesGroupDelay)
{
    constexpr int sourceRate = 44100;
    constexpr std::size_t spikeIndex = 10000;

    std::vector<float> input (20000, 0.0f);
    // Narrow triangular spike centred exactly on spikeIndex.
    for (int offset = -4; offset <= 4; ++offset)
    {
        const auto index = static_cast<std::size_t> (
            static_cast<long long> (spikeIndex) + offset);
        const double weight = 1.0 - std::fabs (static_cast<double> (offset)) / 5.0;
        input[index] = static_cast<float> (weight);
    }

    const tmix::Decimator decimator (sourceRate, 22050);
    const auto output = decimator.process (input);

    std::size_t peakIndex = 0;
    double peakValue = 0.0;
    for (std::size_t i = 0; i < output.size(); ++i)
    {
        if (std::fabs (static_cast<double> (output[i])) > peakValue)
        {
            peakValue = std::fabs (static_cast<double> (output[i]));
            peakIndex = i;
        }
    }

    const std::size_t expectedIndex = spikeIndex / static_cast<std::size_t> (decimator.factor());

    // Allow a couple of output samples: the spike is narrow relative to the
    // filter's transition band.
    const long long error = static_cast<long long> (peakIndex)
                            - static_cast<long long> (expectedIndex);
    TMIX_CHECK (std::llabs (error) <= 2);
    TMIX_CHECK (peakValue > 0.3);
}

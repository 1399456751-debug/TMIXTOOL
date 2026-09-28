#include "OnsetDetector.h"

#include "Fft.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace tmix {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Log compression keeps a loud kick from swamping a quiet hat in the same
// flux sum; the constant sets where the compression starts to bite.
//
// Measured on the kick/hat fixtures: raising this to 1000 flattens the
// difference between bands badly, which costs the tempo stage the evidence it
// needs to tell a kick from a hat. 100 keeps loud material from dominating.
constexpr float kCompressionGain = 100.0f;

std::vector<float> hannWindow (std::size_t size)
{
    std::vector<float> window (size);
    if (size < 2)
        return window;

    for (std::size_t i = 0; i < size; ++i)
    {
        const double angle = 2.0 * kPi * static_cast<double> (i)
                             / static_cast<double> (size - 1);
        window[i] = static_cast<float> (0.5 - 0.5 * std::cos (angle));
    }
    return window;
}

} // namespace

OnsetEnvelope computeOnsetEnvelope (const std::vector<float>& mono,
                                    int sampleRate,
                                    const OnsetConfig& config)
{
    OnsetEnvelope envelope;

    if (sampleRate <= 0 || config.fftSize <= 0 || config.hopSize <= 0)
        return envelope;

    const std::size_t fftSize = static_cast<std::size_t> (config.fftSize);
    const std::size_t hopSize = static_cast<std::size_t> (config.hopSize);
    const std::size_t bins    = fftSize / 2 + 1;

    std::size_t frameCount = 0;
    if (mono.size() >= fftSize)
        frameCount = (mono.size() - fftSize) / hopSize + 1;

    envelope.hopSeconds  = static_cast<double> (hopSize) / static_cast<double> (sampleRate);
    envelope.frameRate   = 1.0 / envelope.hopSeconds;

    // Frame t's flux measures what changed between frames t-1 and t, which is
    // carried entirely by the hop of samples newly admitted at the end of the
    // window. Timestamping the frame at the START of that hop attributes the
    // change to where it entered.
    envelope.frameOffset = (static_cast<double> (fftSize) - static_cast<double> (hopSize))
                           / static_cast<double> (sampleRate);

    envelope.full.assign (frameCount, 0.0f);
    envelope.low.assign (frameCount, 0.0f);
    envelope.high.assign (frameCount, 0.0f);

    if (frameCount == 0)
        return envelope;

    const Fft fft (fftSize);
    const auto window = hannWindow (fftSize);
    const double binHz = static_cast<double> (sampleRate) / static_cast<double> (fftSize);

    // Band edges in bins. Bin 0 is DC and is excluded: a constant offset in the
    // source would otherwise register as an onset in frame 1.
    const std::size_t lowEnd = std::min (
        bins, static_cast<std::size_t> (config.lowBandSplitHz / binHz) + 1);
    const std::size_t highStart = std::min (
        bins, static_cast<std::size_t> (config.highBandSplitHz / binHz));

    std::vector<std::complex<float>> spectrum (fftSize);
    std::vector<float> previous (bins, 0.0f);
    std::vector<float> current (bins, 0.0f);

    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        const std::size_t start = frame * hopSize;
        for (std::size_t i = 0; i < fftSize; ++i)
            spectrum[i] = { mono[start + i] * window[i], 0.0f };

        fft.forward (spectrum.data());

        for (std::size_t k = 0; k < bins; ++k)
            current[k] = std::log (1.0f + kCompressionGain * std::abs (spectrum[k]));

        if (frame > 0)
        {
            float fullSum = 0.0f;
            float lowSum  = 0.0f;
            float highSum = 0.0f;

            for (std::size_t k = 1; k < bins; ++k)
            {
                // SuperFlux reference: the strongest of the neighbouring bins in
                // the previous frame. Cheap, and it removes the false onsets that
                // vibrato and slow pitch drift would otherwise create.
                float reference = previous[k - 1];
                reference = std::max (reference, previous[k]);
                if (k + 1 < bins)
                    reference = std::max (reference, previous[k + 1]);

                const float flux = std::max (0.0f, current[k] - reference);

                fullSum += flux;
                if (k < lowEnd)
                    lowSum += flux;
                if (k >= highStart)
                    highSum += flux;
            }

            envelope.full[frame] = fullSum;
            envelope.low[frame]  = lowSum;
            envelope.high[frame] = highSum;
        }

        previous.swap (current);
    }

    return envelope;
}

} // namespace tmix

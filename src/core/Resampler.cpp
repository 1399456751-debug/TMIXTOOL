#include "Resampler.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tmix {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Kernel half-width in input samples. Wide enough that the transition band
// clears the cutoff well before the target Nyquist: a Blackman window's
// transition width is roughly 3/W of the sample rate, so W=64 puts the
// stopband edge at 0.047 above the cutoff.
constexpr int kHalfWidth = 64;

int tapsFor() { return 2 * kHalfWidth + 1; }

} // namespace

Resampler::Resampler (int sourceRate, int targetRate)
{
    if (sourceRate <= 0 || targetRate <= 0)
        throw std::invalid_argument ("Resampler: sample rates must be positive");

    sourceRate_ = sourceRate;
    targetRate_ = targetRate;
    halfWidth_  = kHalfWidth;

    // Cutoff at the lower of the two Nyquists. When downsampling it is the
    // target's Nyquist that binds, which is what stops content above it from
    // folding back into the audible band.
    const double ratio = static_cast<double> (targetRate_) / static_cast<double> (sourceRate_);
    cutoff_ = static_cast<float> (0.5 * std::min (1.0, ratio));

    const int taps = tapsFor();

    kernel_.assign (static_cast<std::size_t> (kPhases) * static_cast<std::size_t> (taps), 0.0f);

    for (int phase = 0; phase < kPhases; ++phase)
    {
        const double fraction = static_cast<double> (phase) / static_cast<double> (kPhases);

        double sum = 0.0;

        for (int k = 0; k < taps; ++k)
        {
            // Distance from the interpolation point, in input samples.
            const double x = fraction + static_cast<double> (halfWidth_ - k);

            const double argument = 2.0 * static_cast<double> (cutoff_) * x;
            const double sincValue = std::fabs (argument) < 1e-12
                ? 1.0
                : std::sin (kPi * argument) / (kPi * argument);

            // Blackman window over [-1, 1].
            const double normalised = x / static_cast<double> (halfWidth_);
            const double window = std::fabs (normalised) <= 1.0
                ? 0.42 + 0.5 * std::cos (kPi * normalised)
                       + 0.08 * std::cos (2.0 * kPi * normalised)
                : 0.0;

            const double value = 2.0 * static_cast<double> (cutoff_) * sincValue * window;

            kernel_[static_cast<std::size_t> (phase * taps + k)] =
                static_cast<float> (value);

            sum += value;
        }

        // Unity DC gain for every phase, so the output level does not ripple
        // with the fractional part of the sample position.
        if (std::fabs (sum) > 1e-12)
            for (int k = 0; k < taps; ++k)
            {
                auto& tap = kernel_[static_cast<std::size_t> (phase * taps + k)];
                tap = static_cast<float> (static_cast<double> (tap) / sum);
            }
    }
}

std::size_t Resampler::outputLength (std::size_t inputLength) const noexcept
{
    // Integer arithmetic: the product fits comfortably in 64 bits for any
    // audio file, and it avoids the rounding surprises of a floating division.
    return static_cast<std::size_t> (
        (static_cast<long long> (inputLength) * targetRate_) / sourceRate_);
}

std::vector<float> Resampler::process (const std::vector<float>& input) const
{
    const std::size_t outLength = outputLength (input.size());

    if (sourceRate_ == targetRate_)
        return std::vector<float> (input.begin(),
                                   input.begin() + static_cast<std::ptrdiff_t> (outLength));

    std::vector<float> output (outLength, 0.0f);

    const int taps = tapsFor();
    const double step = static_cast<double> (sourceRate_) / static_cast<double> (targetRate_);
    const auto inputSize = static_cast<long long> (input.size());

    for (std::size_t n = 0; n < outLength; ++n)
    {
        const double position = static_cast<double> (n) * step;
        const double whole = std::floor (position);
        const double fraction = position - whole;

        const auto firstIndex = static_cast<long long> (whole) - halfWidth_;

        auto phase = static_cast<int> (fraction * kPhases);
        if (phase >= kPhases)
            phase = kPhases - 1;

        const float* kernel =
            &kernel_[static_cast<std::size_t> (phase) * static_cast<std::size_t> (taps)];

        double sum = 0.0;
        for (int k = 0; k < taps; ++k)
        {
            const long long index = firstIndex + k;
            if (index >= 0 && index < inputSize)
                sum += static_cast<double> (kernel[k])
                       * static_cast<double> (input[static_cast<std::size_t> (index)]);
        }

        output[n] = static_cast<float> (sum);
    }

    return output;
}

} // namespace tmix

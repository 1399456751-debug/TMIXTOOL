#include "Decimator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace tmix {

namespace {

constexpr double kPi = 3.14159265358979323846;

double sinc (double x)
{
    if (std::fabs (x) < 1e-12)
        return 1.0;
    const double pix = kPi * x;
    return std::sin (pix) / pix;
}

// Upper bound on filter length. Keeps the cost of very high rate sources
// (192 kHz, factor 8) from growing without limit.
constexpr std::size_t kMaxTaps = 512;

} // namespace

Decimator::Decimator (int sourceRate, int targetRate)
{
    if (sourceRate <= 0 || targetRate <= 0)
        throw std::invalid_argument ("Decimator: sample rates must be positive");

    sourceRate_ = sourceRate;
    factor_     = std::max (1, sourceRate / targetRate);
    outputRate_ = sourceRate_ / factor_;

    if (factor_ == 1)
        return; // Nothing to filter: the signal is already at the analysis rate.

    const double outputNyquist = static_cast<double> (outputRate_) / 2.0;
    const double cutoff        = 0.45 * static_cast<double> (outputRate_);
    const double transition    = outputNyquist - cutoff;

    // Hamming-windowed sinc: N ~= 3.3 / (transition / sourceRate), forced odd so
    // the filter has a single centre tap and an integer group delay.
    std::size_t tapCount = static_cast<std::size_t> (
        std::ceil (3.3 * static_cast<double> (sourceRate_) / transition));
    tapCount = std::min (tapCount, kMaxTaps);
    if (tapCount % 2 == 0)
        ++tapCount;

    delay_ = static_cast<int> ((tapCount - 1) / 2);

    const double normalisedCutoff = cutoff / static_cast<double> (sourceRate_);

    taps_.resize (tapCount);
    double sum = 0.0;
    for (std::size_t k = 0; k < tapCount; ++k)
    {
        const double offset = static_cast<double> (k) - static_cast<double> (delay_);
        const double window = 0.54 - 0.46 * std::cos (
            2.0 * kPi * static_cast<double> (k) / static_cast<double> (tapCount - 1));

        const double value = 2.0 * normalisedCutoff
                             * sinc (2.0 * normalisedCutoff * offset) * window;

        taps_[k] = static_cast<float> (value);
        sum += value;
    }

    // Unity DC gain, so levels are comparable before and after decimation.
    if (sum != 0.0)
        for (auto& tap : taps_)
            tap = static_cast<float> (static_cast<double> (tap) / sum);
}

std::size_t Decimator::outputLength (std::size_t inputLength) const noexcept
{
    return inputLength / static_cast<std::size_t> (factor_);
}

std::vector<float> Decimator::process (const std::vector<float>& input) const
{
    const std::size_t outLength = outputLength (input.size());

    if (factor_ == 1)
    {
        std::vector<float> passthrough (input.begin(),
                                        input.begin() + static_cast<std::ptrdiff_t> (outLength));
        return passthrough;
    }

    std::vector<float> output (outLength, 0.0f);

    const std::ptrdiff_t tapCount  = static_cast<std::ptrdiff_t> (taps_.size());
    const std::ptrdiff_t inputSize = static_cast<std::ptrdiff_t> (input.size());

    for (std::size_t n = 0; n < outLength; ++n)
    {
        // Output sample n stands for input sample n * factor. The filter is
        // evaluated so that its centre tap lands exactly there, which cancels
        // the group delay; without this every beat would be reported late.
        const std::ptrdiff_t centre =
            static_cast<std::ptrdiff_t> (n) * factor_ + delay_;

        double sum = 0.0;
        for (std::ptrdiff_t k = 0; k < tapCount; ++k)
        {
            const std::ptrdiff_t index = centre - k;
            if (index >= 0 && index < inputSize)
                sum += static_cast<double> (taps_[static_cast<std::size_t> (k)])
                       * static_cast<double> (input[static_cast<std::size_t> (index)]);
        }

        output[n] = static_cast<float> (sum);
    }

    return output;
}

} // namespace tmix

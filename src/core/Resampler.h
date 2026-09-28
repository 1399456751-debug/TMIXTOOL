#pragma once

#include <cstddef>
#include <vector>

namespace tmix {

// Bandlimited resampling to an arbitrary target rate.
//
// The neural front end requires exactly 22050 Hz, and 48000 -> 22050 is a ratio
// of 147/320 - not an integer. Integer decimation therefore cannot produce it,
// and feeding the model 24000 Hz data would shift every mel band by about a
// semitone and a half. A windowed-sinc interpolator handles any ratio.
class Resampler
{
public:
    // Throws std::invalid_argument if either rate is not positive.
    Resampler (int sourceRate, int targetRate);

    int sourceRate() const noexcept { return sourceRate_; }
    int targetRate() const noexcept { return targetRate_; }

    std::size_t outputLength (std::size_t inputLength) const noexcept;

    // Output sample n represents input time n * sourceRate / targetRate.
    std::vector<float> process (const std::vector<float>& input) const;

private:
    int   sourceRate_ = 0;
    int   targetRate_ = 0;
    int   halfWidth_  = 0; // kernel half-width in input samples
    float cutoff_     = 0.0f; // normalised cutoff, cycles per input sample

    // Kernel sampled at a fixed set of sub-sample offsets, so the inner loop
    // is a plain table lookup rather than a sin() per tap.
    static constexpr int kPhases = 128;
    std::vector<float> kernel_; // kPhases * (2 * halfWidth_ + 1)
};

} // namespace tmix

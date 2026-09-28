#pragma once

#include <cstddef>
#include <vector>

namespace tmix {

// Per-frame onset strength. The band split lets the tempo stage reason about
// which events carry the pulse: a kick on the beat and a hat between beats
// look identical in a single full-band curve but not when separated.
struct OnsetEnvelope
{
    double frameRate    = 0.0; // frames per second
    double hopSeconds   = 0.0;
    double frameOffset  = 0.0; // time of frame 0 in seconds (window centre)

    std::vector<float> full; // all bands
    std::vector<float> low;  // below lowBandSplitHz
    std::vector<float> high; // above highBandSplitHz

    std::size_t frameCount() const noexcept { return full.size(); }

    // Time of a frame in seconds.
    double frameTime (std::size_t frame) const noexcept
    {
        return frameOffset + static_cast<double> (frame) * hopSeconds;
    }
};

struct OnsetConfig
{
    int fftSize   = 2048;
    int hopSize   = 512;
    double lowBandSplitHz  = 200.0;
    double highBandSplitHz = 4000.0;
};

// Computes a SuperFlux-style onset strength curve from a mono signal.
//
// The spectral difference is taken against the maximum of the three
// neighbouring bins in the previous frame rather than against the same bin
// alone. That costs nothing and suppresses the false onsets that vibrato and
// slow pitch movement otherwise produce.
OnsetEnvelope computeOnsetEnvelope (const std::vector<float>& mono,
                                    int sampleRate,
                                    const OnsetConfig& config = {});

} // namespace tmix

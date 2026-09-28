#pragma once

#include <cstddef>
#include <vector>

namespace tmix {

// Integer-factor decimator with an anti-aliasing low-pass.
//
// An integer factor is used rather than arbitrary rational resampling because
// it is exact: no interpolation phase drift, and the analysis rate stays tied
// to the source rate so results are comparable across 44.1/48/96 kHz material.
//
// The FIR is linear phase, and its group delay is compensated so that output
// sample n corresponds to input sample n * factor(). Without that compensation
// every downstream timing estimate - beat positions in particular - would be
// late by the filter's group delay.
class Decimator
{
public:
    // Picks the largest integer factor whose output rate still meets or
    // exceeds targetRate. Throws std::invalid_argument if either rate is
    // not positive.
    Decimator (int sourceRate, int targetRate);

    int factor() const noexcept { return factor_; }
    int sourceRate() const noexcept { return sourceRate_; }
    int outputRate() const noexcept { return outputRate_; }

    std::size_t outputLength (std::size_t inputLength) const noexcept;

    std::vector<float> process (const std::vector<float>& input) const;

    // Number of taps in the anti-aliasing filter (1 or 0 for pass-through).
    std::size_t tapCount() const noexcept { return taps_.size(); }

private:
    int   factor_     = 1;
    int   sourceRate_ = 0;
    int   outputRate_ = 0;
    int   delay_      = 0; // group delay in input samples
    std::vector<float> taps_;
};

} // namespace tmix

#include "TestHarness.h"

#include "Fft.h"

#include <cmath>
#include <complex>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

using Complex = std::complex<double>;

// Deliberately naive O(N^2) DFT. It exists only as ground truth for the fast
// transform, so it is written for obvious correctness rather than speed.
std::vector<Complex> naiveDft (const std::vector<Complex>& in)
{
    const std::size_t n = in.size();
    std::vector<Complex> out (n);

    for (std::size_t k = 0; k < n; ++k)
    {
        Complex sum (0.0, 0.0);
        for (std::size_t t = 0; t < n; ++t)
        {
            const double angle =
                -2.0 * kPi * static_cast<double> (k) * static_cast<double> (t)
                / static_cast<double> (n);
            sum += in[t] * Complex (std::cos (angle), std::sin (angle));
        }
        out[k] = sum;
    }

    return out;
}

// Deterministic pseudo-random complex signal. A fixed recurrence rather than
// <random> keeps the fixture identical across standard library versions.
std::vector<Complex> pseudoRandomSignal (std::size_t n, std::uint32_t seed)
{
    std::vector<Complex> out (n);
    std::uint32_t state = seed;

    auto next = [&state]() {
        state = state * 1664525u + 1013904223u;
        return static_cast<double> (state >> 8) / static_cast<double> (1u << 24) - 0.5;
    };

    for (std::size_t i = 0; i < n; ++i)
        out[i] = Complex (next(), next());

    return out;
}

} // namespace

TMIX_TEST (Fft_RejectsNonPowerOfTwoSize)
{
    bool threwOnHundred = false;
    try { const tmix::Fft fft (100); }
    catch (const std::invalid_argument&) { threwOnHundred = true; }
    TMIX_CHECK (threwOnHundred);

    bool threwOnZero = false;
    try { const tmix::Fft fft (0); }
    catch (const std::invalid_argument&) { threwOnZero = true; }
    TMIX_CHECK (threwOnZero);
}

// A constant signal has all of its energy in bin zero, at value N.
TMIX_TEST (Fft_DcInputLandsEntirelyInFirstBin)
{
    constexpr std::size_t n = 64;
    const tmix::Fft fft (n);

    std::vector<std::complex<float>> data (n, { 1.0f, 0.0f });
    fft.forward (data.data());

    TMIX_CHECK_NEAR (data[0].real(), static_cast<double> (n), 1e-3);
    TMIX_CHECK_NEAR (data[0].imag(), 0.0, 1e-3);

    for (std::size_t k = 1; k < n; ++k)
        TMIX_CHECK_NEAR (std::abs (data[k]), 0.0, 1e-3);
}

// A pure sine at exactly bin k puts a pair of peaks at k and N-k.
TMIX_TEST (Fft_SingleSinePeaksInExpectedBin)
{
    constexpr std::size_t n    = 128;
    constexpr std::size_t bin  = 7;
    const tmix::Fft fft (n);

    std::vector<std::complex<float>> data (n);
    for (std::size_t t = 0; t < n; ++t)
    {
        const double angle = 2.0 * kPi * static_cast<double> (bin)
                             * static_cast<double> (t) / static_cast<double> (n);
        data[t] = { static_cast<float> (std::sin (angle)), 0.0f };
    }

    fft.forward (data.data());

    // A real sine of amplitude 1 splits into two complex exponentials of
    // amplitude N/2 each.
    TMIX_CHECK_NEAR (std::abs (data[bin]), static_cast<double> (n) / 2.0, 1e-2);
    TMIX_CHECK_NEAR (std::abs (data[n - bin]), static_cast<double> (n) / 2.0, 1e-2);

    // Everything else must be negligible.
    for (std::size_t k = 0; k < n; ++k)
    {
        if (k == bin || k == n - bin)
            continue;
        TMIX_CHECK_NEAR (std::abs (data[k]), 0.0, 1e-2);
    }
}

// The strongest check: every output bin must match the naive transform. This
// catches twiddle-factor, bit-reversal and stage-count mistakes that a
// single-sine test can miss.
TMIX_TEST (Fft_MatchesNaiveDft)
{
    constexpr std::size_t n = 128;
    const tmix::Fft fft (n);

    const auto signal = pseudoRandomSignal (n, 12345u);
    const auto expected = naiveDft (signal);

    std::vector<std::complex<float>> actual (n);
    for (std::size_t i = 0; i < n; ++i)
        actual[i] = { static_cast<float> (signal[i].real()),
                      static_cast<float> (signal[i].imag()) };

    fft.forward (actual.data());

    for (std::size_t k = 0; k < n; ++k)
    {
        TMIX_CHECK_NEAR (actual[k].real(), expected[k].real(), 1e-3);
        TMIX_CHECK_NEAR (actual[k].imag(), expected[k].imag(), 1e-3);
    }
}

// inverse(forward(x)) must return x, which is what every windowed analysis
// round trip depends on.
TMIX_TEST (Fft_InverseRoundTrips)
{
    constexpr std::size_t n = 64;
    const tmix::Fft fft (n);

    const auto original = pseudoRandomSignal (n, 999u);

    std::vector<std::complex<float>> data (n);
    for (std::size_t i = 0; i < n; ++i)
        data[i] = { static_cast<float> (original[i].real()),
                    static_cast<float> (original[i].imag()) };

    fft.forward (data.data());

    // Guard against a no-op forward transform passing this test vacuously:
    // the spectrum must actually differ from the input.
    double change = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        change += std::abs (static_cast<double> (data[i].real()) - original[i].real());
    TMIX_CHECK (change > 1.0);

    fft.inverse (data.data());

    for (std::size_t i = 0; i < n; ++i)
    {
        TMIX_CHECK_NEAR (data[i].real(), original[i].real(), 1e-4);
        TMIX_CHECK_NEAR (data[i].imag(), original[i].imag(), 1e-4);
    }
}

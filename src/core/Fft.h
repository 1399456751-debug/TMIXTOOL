#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace tmix {

// Iterative radix-2 complex FFT with precomputed bit-reversal and twiddle
// tables. Built here rather than pulled from a library so the analysis core
// keeps zero third-party dependencies; it is verified against a naive DFT.
class Fft
{
public:
    // Throws std::invalid_argument if size is zero or not a power of two.
    explicit Fft (std::size_t size);

    std::size_t size() const noexcept { return size_; }

    // Forward DFT, in place. `data` must hold size() complex values.
    void forward (std::complex<float>* data) const;

    // Inverse DFT, in place, including the 1/N scaling.
    void inverse (std::complex<float>* data) const;

private:
    void transform (std::complex<float>* data, bool invert) const;

    std::size_t                      size_ = 0;
    std::vector<std::size_t>         bitReverse_;
    std::vector<std::complex<float>> twiddles_;
};

} // namespace tmix

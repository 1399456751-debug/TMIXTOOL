#include "Fft.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace tmix {

namespace {

constexpr double kPi = 3.14159265358979323846;

bool isPowerOfTwo (std::size_t value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

} // namespace

Fft::Fft (std::size_t size)
    : size_ (size)
{
    if (! isPowerOfTwo (size_))
        throw std::invalid_argument ("Fft: size must be a non-zero power of two");

    std::size_t bits = 0;
    while ((std::size_t (1) << bits) < size_)
        ++bits;

    bitReverse_.resize (size_);
    for (std::size_t i = 0; i < size_; ++i)
    {
        std::size_t reversed = 0;
        for (std::size_t b = 0; b < bits; ++b)
            if (i & (std::size_t (1) << b))
                reversed |= std::size_t (1) << (bits - 1 - b);

        bitReverse_[i] = reversed;
    }

    // Forward-transform twiddles: exp(-2*pi*i*k/N) for k in [0, N/2).
    twiddles_.resize (size_ / 2);
    for (std::size_t k = 0; k < size_ / 2; ++k)
    {
        const double angle = -2.0 * kPi * static_cast<double> (k)
                             / static_cast<double> (size_);
        twiddles_[k] = { static_cast<float> (std::cos (angle)),
                         static_cast<float> (std::sin (angle)) };
    }
}

void Fft::forward (std::complex<float>* data) const
{
    transform (data, false);
}

void Fft::inverse (std::complex<float>* data) const
{
    transform (data, true);
}

void Fft::transform (std::complex<float>* data, bool invert) const
{
    for (std::size_t i = 0; i < size_; ++i)
    {
        const std::size_t j = bitReverse_[i];
        if (i < j)
            std::swap (data[i], data[j]);
    }

    for (std::size_t len = 2; len <= size_; len <<= 1)
    {
        const std::size_t half = len / 2;
        const std::size_t step = size_ / len;

        for (std::size_t base = 0; base < size_; base += len)
        {
            for (std::size_t j = 0; j < half; ++j)
            {
                std::complex<float> w = twiddles_[j * step];
                if (invert)
                    w = std::conj (w);

                const std::complex<float> u = data[base + j];
                const std::complex<float> v = data[base + j + half] * w;

                data[base + j]        = u + v;
                data[base + j + half] = u - v;
            }
        }
    }

    if (invert)
    {
        const float scale = 1.0f / static_cast<float> (size_);
        for (std::size_t i = 0; i < size_; ++i)
            data[i] *= scale;
    }
}

} // namespace tmix

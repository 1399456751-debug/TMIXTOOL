#pragma once

// Test-only helper: writes a minimal RIFF/WAVE file with 16-bit PCM samples.
// Deliberately hand-rolled so the tests do not depend on the production
// decoder to build their own inputs.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace tmixsupport {

namespace detail {

inline void putU32 (std::ofstream& out, std::uint32_t value)
{
    const char bytes[4] = {
        static_cast<char> (value & 0xFF),
        static_cast<char> ((value >> 8) & 0xFF),
        static_cast<char> ((value >> 16) & 0xFF),
        static_cast<char> ((value >> 24) & 0xFF),
    };
    out.write (bytes, 4);
}

inline void putU16 (std::ofstream& out, std::uint16_t value)
{
    const char bytes[2] = {
        static_cast<char> (value & 0xFF),
        static_cast<char> ((value >> 8) & 0xFF),
    };
    out.write (bytes, 2);
}

} // namespace detail

// interleaved holds frameCount * channels samples in [-1, 1].
inline bool writeWav16 (const std::filesystem::path& path,
                        int sampleRate,
                        int channels,
                        const std::vector<float>& interleaved)
{
    if (sampleRate <= 0 || channels <= 0)
        return false;

    std::ofstream out (path, std::ios::binary);
    if (! out)
        return false;

    const std::uint16_t bitsPerSample = 16;
    const std::uint16_t blockAlign =
        static_cast<std::uint16_t> (channels * bitsPerSample / 8);
    const std::uint32_t byteRate =
        static_cast<std::uint32_t> (sampleRate) * blockAlign;
    const std::uint32_t dataBytes =
        static_cast<std::uint32_t> (interleaved.size() * sizeof (std::int16_t));

    out.write ("RIFF", 4);
    detail::putU32 (out, 36 + dataBytes);
    out.write ("WAVE", 4);

    out.write ("fmt ", 4);
    detail::putU32 (out, 16);
    detail::putU16 (out, 1); // PCM
    detail::putU16 (out, static_cast<std::uint16_t> (channels));
    detail::putU32 (out, static_cast<std::uint32_t> (sampleRate));
    detail::putU32 (out, byteRate);
    detail::putU16 (out, blockAlign);
    detail::putU16 (out, bitsPerSample);

    out.write ("data", 4);
    detail::putU32 (out, dataBytes);

    for (const float sample : interleaved)
    {
        const float clamped = sample > 1.0f ? 1.0f : (sample < -1.0f ? -1.0f : sample);
        // Scale so that -1.0 maps to -32767 rather than overflowing to -32768
        // symmetry, keeping round-trip error predictable.
        const auto quantised = static_cast<std::int16_t> (clamped * 32767.0f);
        detail::putU16 (out, static_cast<std::uint16_t> (quantised));
    }

    return out.good();
}

} // namespace tmixsupport

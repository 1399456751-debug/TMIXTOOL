#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tmix {

// A whole audio file held in memory as interleaved float samples in [-1, 1].
struct DecodedAudio
{
    int  sampleRate = 0;
    int  channels   = 0;
    std::vector<float> samples;

    std::int64_t frameCount() const noexcept
    {
        return channels > 0 ? static_cast<std::int64_t> (samples.size()) / channels : 0;
    }

    double durationSeconds() const noexcept
    {
        return sampleRate > 0 ? static_cast<double> (frameCount()) / sampleRate : 0.0;
    }
};

// Decodes an entire audio file (WAV or MP3) into memory. Returns false and
// fills `error` on failure - a missing or malformed file is an expected
// condition, not an exception.
//
// The miniaudio dependency is deliberately kept behind this interface: no
// third-party type appears in this header, so the backend can be replaced
// without touching consumers.
bool decodeAudioFile (const std::filesystem::path& path,
                      DecodedAudio& out,
                      std::string& error);

} // namespace tmix

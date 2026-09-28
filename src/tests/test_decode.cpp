#include "TestHarness.h"

#include "support/WavWrite.h"

#include "AudioFile.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

// Temp files that clean themselves up when the case ends.
struct TempWav
{
    std::filesystem::path path;

    explicit TempWav (const std::string& name)
        : path (std::filesystem::temp_directory_path() / name)
    {
        std::error_code ec;
        std::filesystem::remove (path, ec);
    }

    ~TempWav()
    {
        std::error_code ec;
        std::filesystem::remove (path, ec);
    }
};

} // namespace

// A decoded file must report the sample rate and channel count the encoder
// wrote, and expose exactly the frames that were encoded.
TMIX_TEST (Decode_ReadsWavProperties)
{
    constexpr int sampleRate = 44100;
    constexpr int channels   = 2;
    constexpr int frames     = 1000;

    std::vector<float> samples (static_cast<std::size_t> (frames) * channels, 0.0f);
    for (int f = 0; f < frames; ++f)
    {
        samples[static_cast<std::size_t> (f) * channels + 0] = 0.5f;
        samples[static_cast<std::size_t> (f) * channels + 1] = -0.25f;
    }

    const TempWav temp ("tmixtool_decode_properties.wav");
    TMIX_REQUIRE (tmixsupport::writeWav16 (temp.path, sampleRate, channels, samples));

    tmix::DecodedAudio decoded;
    std::string error;
    const bool ok = tmix::decodeAudioFile (temp.path, decoded, error);
    TMIX_CHECK (ok);
    if (! ok)
        std::printf ("      decode error: %s\n", error.c_str());

    TMIX_CHECK_EQ (decoded.sampleRate, sampleRate);
    TMIX_CHECK_EQ (decoded.channels, channels);
    TMIX_CHECK_EQ (static_cast<int> (decoded.frameCount()), frames);
}

// Sample values must survive the round trip within 16-bit quantisation error.
TMIX_TEST (Decode_PreservesSampleValues)
{
    constexpr int sampleRate = 48000;
    constexpr int channels   = 1;
    constexpr int frames     = 256;

    std::vector<float> samples (frames);
    for (int f = 0; f < frames; ++f)
    {
        const double value = 0.8 * std::sin (2.0 * 3.14159265358979 * 440.0 * f / sampleRate);
        samples[static_cast<std::size_t> (f)] = static_cast<float> (value);
    }

    const TempWav temp ("tmixtool_decode_values.wav");
    TMIX_REQUIRE (tmixsupport::writeWav16 (temp.path, sampleRate, channels, samples));

    tmix::DecodedAudio decoded;
    std::string error;
    TMIX_REQUIRE (tmix::decodeAudioFile (temp.path, decoded, error));

    // Required rather than checked: the loop below indexes into this container,
    // so a failed size check must abandon the case instead of running off the end.
    TMIX_REQUIRE (decoded.samples.size() == static_cast<std::size_t> (frames));

    // 16-bit quantisation step is 1/32767; allow a little headroom.
    constexpr double tolerance = 2.0 / 32767.0;
    for (int f = 0; f < frames; ++f)
    {
        TMIX_CHECK_NEAR (decoded.samples[static_cast<std::size_t> (f)],
                         samples[static_cast<std::size_t> (f)],
                         tolerance);
    }
}

// A missing or unreadable file is an expected condition, not a crash.
TMIX_TEST (Decode_ReportsMissingFile)
{
    tmix::DecodedAudio decoded;
    std::string error;

    const bool ok = tmix::decodeAudioFile (
        std::filesystem::temp_directory_path() / "tmixtool_no_such_file_12345.wav",
        decoded, error);

    TMIX_CHECK (! ok);
    TMIX_CHECK (! error.empty());
}

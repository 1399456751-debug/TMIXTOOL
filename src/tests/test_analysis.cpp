#include "TestHarness.h"

#include "support/Signals.h"
#include "support/WavWrite.h"

#include "Analysis.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr int kSourceRate = 44100;

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

// End to end: a file on disk in, a tempo out. This is the path the CLI and the
// GUI both take, so it is worth exercising as a whole rather than only in
// pieces.
TMIX_TEST (Analysis_RecoversTempoFromFile)
{
    constexpr double bpm = 128.0;

    const auto signal = tmixsupport::clickTrack (bpm, kSourceRate, 15.0);

    const TempWav temp ("tmixtool_analysis_tempo.wav");
    TMIX_REQUIRE (tmixsupport::writeWav16 (temp.path, kSourceRate, 1, signal));

    const auto result = tmix::analyseFile (temp.path);

    TMIX_REQUIRE (result.ok);

    TMIX_CHECK_EQ (result.sampleRate, kSourceRate);
    TMIX_CHECK_EQ (result.channels, 1);
    TMIX_CHECK_EQ (static_cast<int> (result.frameCount),
                   static_cast<int> (signal.size()));

    TMIX_REQUIRE (result.tempo.valid);
    if (result.tempo.valid && std::fabs (result.tempo.bpm - bpm) > 0.5)
        std::printf ("      got %.4f, wanted %.1f\n", result.tempo.bpm, bpm);

    TMIX_CHECK_NEAR (result.tempo.bpm, bpm, 0.5);
}

// Levels must be reported from the source, before any downmix or decimation.
TMIX_TEST (Analysis_ReportsSourceLevels)
{
    const auto signal = tmixsupport::clickTrack (120.0, kSourceRate, 5.0);

    const TempWav temp ("tmixtool_analysis_levels.wav");
    TMIX_REQUIRE (tmixsupport::writeWav16 (temp.path, kSourceRate, 1, signal));

    const auto result = tmix::analyseFile (temp.path);

    TMIX_REQUIRE (result.ok);

    TMIX_CHECK (result.levels.peak > 0.5);
    TMIX_CHECK (result.levels.peak <= 1.0);
    TMIX_CHECK (result.levels.rms > 0.0);
    TMIX_CHECK (result.levels.rms < result.levels.peak);
}

// A file that cannot be read is reported, not crashed on.
TMIX_TEST (Analysis_ReportsUnreadableFile)
{
    const auto result = tmix::analyseFile (
        std::filesystem::temp_directory_path() / "tmixtool_analysis_missing_98765.wav");

    TMIX_CHECK (! result.ok);
    TMIX_CHECK (! result.error.empty());
}

// Non-ASCII paths are the norm for this user's library, and they are where
// naive narrow-string handling silently fails.
TMIX_TEST (Analysis_HandlesNonAsciiPath)
{
    constexpr double bpm = 120.0;

    const auto signal = tmixsupport::clickTrack (bpm, kSourceRate, 12.0);

    const TempWav temp ("tmixtool_\xE6\xB5\x8B\xE8\xAF\x95 \xE9\x9F\xB3\xE9\xA2\x91 01.wav");
    TMIX_REQUIRE (tmixsupport::writeWav16 (temp.path, kSourceRate, 1, signal));

    const auto result = tmix::analyseFile (temp.path);

    TMIX_REQUIRE (result.ok);
    TMIX_CHECK_NEAR (result.tempo.bpm, bpm, 0.5);
}

// The progress callback must be honoured and must be able to cancel.
TMIX_TEST (Analysis_HonoursCancellation)
{
    const auto signal = tmixsupport::clickTrack (120.0, kSourceRate, 5.0);

    const TempWav temp ("tmixtool_analysis_cancel.wav");
    TMIX_REQUIRE (tmixsupport::writeWav16 (temp.path, kSourceRate, 1, signal));

    int calls = 0;
    const auto result = tmix::analyseFile (
        temp.path, {},
        [&calls] (float, const char*) { ++calls; return false; }); // cancel immediately

    TMIX_CHECK (calls > 0);
    TMIX_CHECK (! result.ok);
}

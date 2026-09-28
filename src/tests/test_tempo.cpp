#include "TestHarness.h"

#include "support/Signals.h"

#include "Decimator.h"
#include "OnsetDetector.h"
#include "TempoEstimator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>
#include <cstdio>

namespace {

constexpr int kSourceRate = 44100;

double medianOf (std::vector<double> values)
{
    if (values.empty())
        return 0.0;

    const std::size_t middle = values.size() / 2;
    std::nth_element (values.begin(),
                      values.begin() + static_cast<std::ptrdiff_t> (middle),
                      values.end());
    return values[middle];
}


// Wide range: the accuracy tests are about what the search finds, so the
// octave folding is deliberately switched off for them.
tmix::TempoConfig wideRange()
{
    tmix::TempoConfig config;
    config.preferredMinBpm = 40.0;
    config.preferredMaxBpm = 220.0;
    return config;
}

// The full front end: decimate, then onset envelope.
tmix::OnsetEnvelope analyse (const std::vector<float>& signal)
{
    const tmix::Decimator decimator (kSourceRate, 22050);
    const auto analysis = decimator.process (signal);
    return tmix::computeOnsetEnvelope (analysis, decimator.outputRate());
}

} // namespace

// The headline requirement: a click track at a known tempo must come back to
// within a tenth of a beat per minute.
TMIX_TEST (Tempo_ClickTrackIsAccurate)
{
    const auto envelope = analyse (tmixsupport::clickTrack (120.0, kSourceRate, 20.0));

    const auto result = tmix::estimateTempo (envelope, wideRange());

    TMIX_CHECK (result.valid);
    TMIX_CHECK_NEAR (result.bpm, 120.0, 0.1);
}

// Accuracy must hold across the range the user actually works in, not just at
// one convenient tempo.
TMIX_TEST (Tempo_IsAccurateAcrossTheRange)
{
    const double tempos[] = { 60.0, 80.0, 100.0, 120.0, 140.0, 160.0, 180.0 };

    for (const double bpm : tempos)
    {
        const auto envelope = analyse (tmixsupport::clickTrack (bpm, kSourceRate, 20.0));

        const auto result = tmix::estimateTempo (envelope, wideRange());

        TMIX_CHECK (result.valid);
        if (result.valid)
        {
            // Report which tempo failed rather than just "an assertion failed".
            if (std::fabs (result.bpm - bpm) > 0.1)
                std::printf ("      expected %.1f, got %.4f\n", bpm, result.bpm);

            TMIX_CHECK_NEAR (result.bpm, bpm, 0.1);
        }
    }
}

// Silence carries no tempo. Reporting one would be worse than reporting none.
TMIX_TEST (Tempo_SilenceIsNotValid)
{
    const std::vector<float> silence (static_cast<std::size_t> (kSourceRate) * 5, 0.0f);

    const auto result = tmix::estimateTempo (analyse (silence));

    TMIX_CHECK (! result.valid);
}

// The reported grid phase must line up with the actual beats: a click track
// built from t=0 has beats at every multiple of the beat period.
TMIX_TEST (Tempo_BeatPhaseMatchesTheGrid)
{
    constexpr double bpm = 120.0;
    constexpr double leadIn = 0.5;

    auto clicks = tmixsupport::clickTrack (bpm, kSourceRate, 18.0);

    std::vector<float> signal (static_cast<std::size_t> (leadIn * kSourceRate), 0.0f);
    signal.insert (signal.end(), clicks.begin(), clicks.end());

    const auto result = tmix::estimateTempo (analyse (signal));

    TMIX_REQUIRE (result.valid);
    TMIX_CHECK_NEAR (result.bpm, bpm, 0.1);

    // Fold the phase into [0, one beat) and compare against the offset the
    // fixture was built with.
    const double beatSeconds = 60.0 / bpm;
    double phase = std::fmod (result.firstBeatSeconds - leadIn, beatSeconds);
    if (phase < 0.0)
        phase += beatSeconds;

    // Either the beats themselves or the half-beat offset are valid grid
    // phases; only the distance to the nearer one matters.
    const double distance = std::min (phase, beatSeconds - phase);
    if (distance >= 0.05)
        std::printf ("      firstBeat %.4f, phase %.4f, distance %.4f\n",
                     result.firstBeatSeconds, phase, distance);
    TMIX_CHECK (distance < 0.05);
}

// ---- half/double-time traps: the reason this tool exists -------------------
//
// Every case here is material where a naive detector reports double or half
// the tempo the user actually counts. These are the readings the old tools got
// wrong, so they are the ones the rewrite has to get right.

// Kick on 1 and 3, snare on 2 and 4, hats on eighths. The fastest regular
// event stream is twice the beat, and the slowest repeat is half of it.
TMIX_TEST (Tempo_StraightRockLandsOnTheBeat)
{
    const auto envelope = analyse (tmixsupport::straightRock (140.0, kSourceRate, 20.0));

    const auto result = tmix::estimateTempo (envelope, wideRange());

    TMIX_REQUIRE (result.valid);
    if (std::fabs (result.bpm - 140.0) > 0.5)
        std::printf ("      got %.4f, wanted 140\n", result.bpm);

    TMIX_CHECK_NEAR (result.bpm, 140.0, 0.5);
}

// Half-time feel at 90 BPM: kick on 1, snare on 3. The events repeat every two
// beats, so a detector that follows the events reports 45 - outside the search
// range - and one that follows the snare reports half the pulse.
TMIX_TEST (Tempo_HalfTimeFeelKeepsThePulse)
{
    const auto envelope = analyse (tmixsupport::halfTimeFeel (90.0, kSourceRate, 20.0));

    const auto result = tmix::estimateTempo (envelope, wideRange());

    TMIX_REQUIRE (result.valid);
    if (std::fabs (result.bpm - 90.0) > 0.5)
        std::printf ("      got %.4f, wanted 90\n", result.bpm);

    TMIX_CHECK_NEAR (result.bpm, 90.0, 0.5);
}

// Four-on-the-floor with offbeat hats. The densest regular stream runs at
// double the beat, and the answer must still be the kick rate.
TMIX_TEST (Tempo_FourOnTheFloorStaysOnTheKick)
{
    const auto envelope = analyse (tmixsupport::fourOnTheFloor (128.0, kSourceRate, 20.0));

    const auto result = tmix::estimateTempo (envelope, wideRange());

    TMIX_REQUIRE (result.valid);
    if (std::fabs (result.bpm - 128.0) > 0.5)
        std::printf ("      got %.4f, wanted 128\n", result.bpm);

    TMIX_CHECK_NEAR (result.bpm, 128.0, 0.5);
}

// An accent every fourth beat must not drag the answer to a quarter of the
// real tempo.
TMIX_TEST (Tempo_AccentedFourKeepsTheBeat)
{
    const auto envelope = analyse (tmixsupport::accentedFour (120.0, kSourceRate, 20.0));

    const auto result = tmix::estimateTempo (envelope, wideRange());

    TMIX_REQUIRE (result.valid);
    TMIX_CHECK_NEAR (result.bpm, 120.0, 0.5);
}

// Sixteenth-note hats under a kick on every beat.
//
// This is the case that used to come back at three halves of the true tempo.
// 150 is the 3:2 relative of 100, not an octave of it, so the octave-folding
// safety net cannot help. Its grid sits on plenty of real onsets - the hats
// are everywhere - and its two grid halves happen to be filled equally, which
// is exactly the shape that used to win the filling ratio.
TMIX_TEST (Tempo_SixteenthHatsKeepTheBeat)
{
    const auto envelope = analyse (tmixsupport::sixteenthHats (100.0, kSourceRate, 30.0));

    const auto result = tmix::estimateTempo (envelope, wideRange());

    TMIX_REQUIRE (result.valid);
    if (std::fabs (result.rawBpm - 100.0) > 1.0)
        std::printf ("      got %.4f, wanted 100\n", result.rawBpm);

    TMIX_CHECK_NEAR (result.rawBpm, 100.0, 1.0);
}

// White noise has no tempo. Claiming one with high confidence would be worse
// than admitting there is none.
TMIX_TEST (Tempo_NoiseIsNotConfident)
{
    tmixsupport::Noise noise (777u);
    std::vector<float> signal (static_cast<std::size_t> (kSourceRate) * 10);
    for (auto& sample : signal)
        sample = static_cast<float> (0.5 * noise.next());

    const auto result = tmix::estimateTempo (analyse (signal));

    if (result.valid)
        TMIX_CHECK (result.confidence < 0.7);
}


// The octave folding is the deliberate answer to half/double ambiguity: the
// search reports what it found, and the user's declared range decides which
// octave is the useful one.
TMIX_TEST (Tempo_FoldsOctaveIntoPreferredRange)
{
    const auto envelope = analyse (tmixsupport::clickTrack (180.0, kSourceRate, 20.0));

    tmix::TempoConfig config;
    config.preferredMinBpm = 70.0;
    config.preferredMaxBpm = 150.0;

    const auto result = tmix::estimateTempo (envelope, config);

    TMIX_REQUIRE (result.valid);
    TMIX_CHECK_NEAR (result.rawBpm, 180.0, 1.0);
    TMIX_CHECK (result.folded);
    TMIX_CHECK_NEAR (result.bpm, 90.0, 0.5);
}

// Material already inside the range must be left alone.
TMIX_TEST (Tempo_LeavesInRangeTempoUnfolded)
{
    const auto envelope = analyse (tmixsupport::clickTrack (128.0, kSourceRate, 20.0));

    tmix::TempoConfig config;
    config.preferredMinBpm = 70.0;
    config.preferredMaxBpm = 150.0;

    const auto result = tmix::estimateTempo (envelope, config);

    TMIX_REQUIRE (result.valid);
    TMIX_CHECK (! result.folded);
    TMIX_CHECK_NEAR (result.bpm, 128.0, 0.5);
}

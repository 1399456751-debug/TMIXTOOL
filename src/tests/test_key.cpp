#include "TestHarness.h"

#include "support/Signals.h"

#include "KeyDetector.h"

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace {

constexpr int kSourceRate = 22050;

// Triads voiced the way music actually is: a bass note under the chord.
//
// The bass is not decoration. Without it a bare triad is genuinely ambiguous -
// C-E-G correlates just as well with E minor as with C major - and the test
// would be measuring an edge case the tool will never be handed. Real
// arrangements put the root underneath, and that is the cue the detector is
// built to use.
const std::vector<int> kCMajor = { 36, 60, 64, 67 }; // C2 + C E G
const std::vector<int> kAMinor = { 33, 57, 60, 64 }; // A1 + A C E
const std::vector<int> kGMajor = { 43, 55, 59, 62 }; // G2 + G B D
const std::vector<int> kDMinor = { 38, 62, 65, 69 }; // D2 + D F A

} // namespace

// A sustained C major triad must come back as C major. The templates are
// transcribed constants, so this also catches a mistyped profile value.
TMIX_TEST (Key_DetectsCMajorTriad)
{
    const auto signal = tmixsupport::chord (kCMajor, kSourceRate, 6.0);

    const auto result = tmix::detectKey (signal, kSourceRate);

    TMIX_REQUIRE (result.valid);
    if (result.name != "C major")
        std::printf ("      got %s (confidence %.2f)\n",
                     result.name.c_str(), result.confidence);

    TMIX_CHECK (result.name == "C major");
}

// A minor shares every note with C major; telling them apart is the hardest
// part of key detection, so it gets its own case.
TMIX_TEST (Key_DetectsAMinorTriad)
{
    const auto signal = tmixsupport::chord (kAMinor, kSourceRate, 6.0);

    const auto result = tmix::detectKey (signal, kSourceRate);

    TMIX_REQUIRE (result.valid);
    if (result.name != "A minor")
        std::printf ("      got %s (confidence %.2f)\n",
                     result.name.c_str(), result.confidence);

    TMIX_CHECK (result.name == "A minor");
}

// Two more keys, so a profile that happens to favour C cannot pass by luck.
TMIX_TEST (Key_DetectsGMajorAndDMinor)
{
    const auto gMajor = tmix::detectKey (tmixsupport::chord (kGMajor, kSourceRate, 6.0),
                                         kSourceRate);
    TMIX_REQUIRE (gMajor.valid);
    TMIX_CHECK (gMajor.name == "G major");

    const auto dMinor = tmix::detectKey (tmixsupport::chord (kDMinor, kSourceRate, 6.0),
                                         kSourceRate);
    TMIX_REQUIRE (dMinor.valid);
    TMIX_CHECK (dMinor.name == "D minor");
}

// A chord built entirely from a transposed copy of the C major shape must move
// with it - the detector may not simply prefer C.
TMIX_TEST (Key_FollowsTransposition)
{
    const std::vector<int> fMajor = { 41, 53, 57, 60 }; // F2 + F A C

    const auto result = tmix::detectKey (tmixsupport::chord (fMajor, kSourceRate, 6.0),
                                         kSourceRate);

    TMIX_REQUIRE (result.valid);
    if (result.name != "F major")
        std::printf ("      got %s\n", result.name.c_str());

    TMIX_CHECK (result.name == "F major");
}

// Silence carries no key. Reporting one would be worse than reporting none.
TMIX_TEST (Key_SilenceIsNotValid)
{
    const std::vector<float> silence (static_cast<std::size_t> (kSourceRate) * 3, 0.0f);

    const auto result = tmix::detectKey (silence, kSourceRate);

    TMIX_CHECK (! result.valid);
}

// Noise has no tonal centre; the detector should not claim one confidently.
TMIX_TEST (Key_NoiseIsNotConfident)
{
    tmixsupport::Noise noise (4242u);
    std::vector<float> signal (static_cast<std::size_t> (kSourceRate) * 4);
    for (auto& sample : signal)
        sample = static_cast<float> (0.4 * noise.next());

    const auto result = tmix::detectKey (signal, kSourceRate);

    if (result.valid)
        TMIX_CHECK (result.confidence < 0.6);
}

// The runner-up is reported, because relative major and minor genuinely are
// hard to separate and the user is the better judge.
TMIX_TEST (Key_ReportsRunnerUp)
{
    const auto signal = tmixsupport::chord (kCMajor, kSourceRate, 6.0);

    const auto result = tmix::detectKey (signal, kSourceRate);

    TMIX_REQUIRE (result.valid);
    TMIX_CHECK (result.candidates.size() >= 2);

    if (result.candidates.size() >= 2)
    {
        TMIX_CHECK (result.candidates[0].score >= result.candidates[1].score);
        TMIX_CHECK (! result.candidates[1].name.empty());
    }
}

// ---- robustness: the two things that actually break key detection ---------

// Recordings are not always at A440. Tape transfers, older records and
// deliberately detuned productions all land somewhere else, and a chroma built
// on the assumption of concert pitch smears each note across its neighbours.
TMIX_TEST (Key_SurvivesDetunedMaterial)
{
    const double offsets[] = { -40.0, -20.0, 20.0, 40.0 };

    for (const double cents : offsets)
    {
        const auto signal = tmixsupport::chord (kCMajor, kSourceRate, 6.0, cents, false);

        const auto result = tmix::detectKey (signal, kSourceRate);

        TMIX_REQUIRE (result.valid);
        if (result.name != "C major")
            std::printf ("      at %+.0f cents got %s\n", cents, result.name.c_str());

        TMIX_CHECK (result.name == "C major");
    }
}

// Percussion has no pitch, but it still lands in the chroma and dilutes it.
// A mix is mostly not sustained tones, so this matters more than the clean
// case above.
TMIX_TEST (Key_SurvivesHeavyPercussion)
{
    const auto tonal = tmixsupport::chord (kCMajor, kSourceRate, 6.0);

    // Snare-like noise bursts on every eighth note, at a level that competes
    // with the chord.
    tmixsupport::Noise noise (31337u);
    std::vector<float> signal = tonal;
    const auto burstLength = static_cast<std::size_t> (0.05 * kSourceRate);
    const auto spacing = static_cast<std::size_t> (0.25 * kSourceRate);

    for (std::size_t start = 0; start + burstLength < signal.size(); start += spacing)
        for (std::size_t i = 0; i < burstLength; ++i)
        {
            const double envelope = std::exp (-static_cast<double> (i)
                                              / (0.012 * kSourceRate));
            signal[start + i] += static_cast<float> (0.35 * envelope * noise.next());
        }

    const auto result = tmix::detectKey (signal, kSourceRate);

    TMIX_REQUIRE (result.valid);
    if (result.name != "C major")
        std::printf ("      got %s\n", result.name.c_str());

    TMIX_CHECK (result.name == "C major");
}

// The reference pitch is reported alongside the key. A chord detuned by a
// known number of cents must come back as the matching frequency, otherwise
// the number is decorative rather than informative.
TMIX_TEST (Key_ReportsReferencePitch)
{
    const double offsets[] = { 0.0, 25.0, -25.0, 12.0 };

    for (const double cents : offsets)
    {
        const auto signal = tmixsupport::chord (kCMajor, kSourceRate, 6.0, cents, false);

        const auto result = tmix::detectKey (signal, kSourceRate);

        TMIX_REQUIRE (result.valid);
        TMIX_REQUIRE (result.tuningValid);

        const double expected = 440.0 * std::pow (2.0, cents / 1200.0);

        if (std::fabs (result.referenceHz - expected) > 0.6)
            std::printf ("      at %+.0f cents got %.1f Hz, wanted %.1f Hz\n",
                         cents, result.referenceHz, expected);

        TMIX_CHECK_NEAR (result.referenceHz, expected, 0.6);
        TMIX_CHECK_NEAR (result.tuningCents, cents, 3.0);
    }
}

// Concert pitch is the common case and must not be reported as some arbitrary
// offset.
TMIX_TEST (Key_ConcertPitchReadsAs440)
{
    const auto signal = tmixsupport::chord (kCMajor, kSourceRate, 6.0, 0.0, false);

    const auto result = tmix::detectKey (signal, kSourceRate);

    TMIX_REQUIRE (result.valid);
    TMIX_REQUIRE (result.tuningValid);
    TMIX_CHECK_NEAR (result.referenceHz, 440.0, 0.4);
}

// Noise has no reference pitch to report, and claiming one would be worse than
// saying nothing.
TMIX_TEST (Key_NoiseHasNoReferencePitch)
{
    tmixsupport::Noise noise (9182u);
    std::vector<float> signal (static_cast<std::size_t> (kSourceRate) * 4);
    for (auto& sample : signal)
        sample = static_cast<float> (0.4 * noise.next());

    const auto result = tmix::detectKey (signal, kSourceRate);

    if (result.valid)
        TMIX_CHECK (result.confidence < 0.6);
}

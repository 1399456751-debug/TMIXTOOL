#include "TestHarness.h"

#include "MixTiming.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const tmix::TimeValue* find (const std::vector<tmix::TimeValue>& values,
                             const std::string& label)
{
    for (const auto& value : values)
        if (value.label == label)
            return &value;
    return nullptr;
}

void checkAllDescribed (const std::vector<tmix::TimeValue>& values)
{
    for (const auto& value : values)
    {
        TMIX_CHECK (! value.label.empty());
        TMIX_CHECK (! value.use.empty());
        TMIX_CHECK (value.ms >= 0.0);
    }
}

} // namespace

TMIX_TEST (MixTiming_RejectsBadTempo)
{
    bool threw = false;
    try { const tmix::MixTiming timing (0.0); }
    catch (const std::invalid_argument&) { threw = true; }
    TMIX_CHECK (threw);
}

TMIX_TEST (MixTiming_BeatLengthAt120Bpm)
{
    const tmix::MixTiming timing (120.0);

    TMIX_CHECK_NEAR (timing.beatMs(), 500.0, 1e-9);
}

// The classic stereo pairings: the sides differ, which is what stops a delay
// lining up with the beat and disappearing into it.
TMIX_TEST (MixTiming_DelayPresetsHaveStereoPairings)
{
    const tmix::MixTiming timing (120.0);
    const auto presets = timing.delayPresets();

    TMIX_CHECK (presets.size() >= 4);

    bool sawUnequalSides = false;
    for (const auto& preset : presets)
    {
        TMIX_CHECK (! preset.name.empty());
        TMIX_CHECK (preset.leftMs > 0.0);
        TMIX_CHECK (preset.rightMs > 0.0);
        TMIX_CHECK_NEAR (preset.leftHz, 1000.0 / preset.leftMs, 1e-6);
        TMIX_CHECK_NEAR (preset.rightHz, 1000.0 / preset.rightMs, 1e-6);

        if (std::fabs (preset.leftMs - preset.rightMs) > 1e-6)
            sawUnequalSides = true;
    }

    TMIX_CHECK (sawUnequalSides);
}

// A dotted eighth at 120 BPM is 1.5 * 250 = 375 ms.
TMIX_TEST (MixTiming_DottedEighthDelayAt120Bpm)
{
    const tmix::MixTiming timing (120.0);
    const auto presets = timing.delayPresets();

    bool found = false;
    for (const auto& preset : presets)
    {
        if (preset.name != "1/8D + 1/8D")
            continue;

        found = true;
        TMIX_CHECK_NEAR (preset.leftMs, 375.0, 1e-6);
        TMIX_CHECK_NEAR (preset.rightMs, 375.0, 1e-6);
    }

    TMIX_CHECK (found);
}

// Attack and release are not note lengths, and this is the test that keeps it
// that way.
//
// A release expressed as a quarter note is 667 ms at 90 BPM - a value nobody
// would dial into a compressor. Useful releases live in a fixed window
// regardless of tempo, so the table must not move with the beat.
TMIX_TEST (MixTiming_AttackAndReleaseStayInPracticalRanges)
{
    const double tempos[] = { 60.0, 90.0, 120.0, 174.0 };

    for (const double bpm : tempos)
    {
        const tmix::MixTiming timing (bpm);

        const auto attacks = timing.attackSuggestions();
        TMIX_CHECK (attacks.size() >= 5);
        checkAllDescribed (attacks);

        for (const auto& attack : attacks)
        {
            TMIX_CHECK (attack.ms >= 0.05);
            TMIX_CHECK (attack.ms <= 50.0);
        }

        const auto releases = timing.releaseSuggestions();
        TMIX_CHECK (releases.size() >= 6);
        checkAllDescribed (releases);

        for (const auto& release : releases)
        {
            TMIX_CHECK (release.ms >= 10.0);
            TMIX_CHECK (release.ms <= 600.0);
        }
    }
}

// Practically useful values differ between tempos even though the table does
// not: the point is that the numbers are chosen for the compressor, not
// computed from the bar.
TMIX_TEST (MixTiming_ReleaseTableDoesNotMoveWithTempo)
{
    const tmix::MixTiming slow (60.0);
    const tmix::MixTiming fast (174.0);

    const auto slowReleases = slow.releaseSuggestions();
    const auto fastReleases = fast.releaseSuggestions();

    TMIX_REQUIRE (slowReleases.size() == fastReleases.size());

    for (std::size_t i = 0; i < slowReleases.size(); ++i)
        TMIX_CHECK_NEAR (slowReleases[i].ms, fastReleases[i].ms, 1e-9);
}

// Beat-synced release is a separate, narrower technique: short note values
// only, and it does move with the tempo.
TMIX_TEST (MixTiming_SyncedReleaseFollowsTheTempoAndStaysShort)
{
    const tmix::MixTiming slow (90.0);
    const tmix::MixTiming fast (140.0);

    const auto slowSync = slow.releaseSyncedOptions();
    const auto fastSync = fast.releaseSyncedOptions();

    TMIX_CHECK (slowSync.size() >= 3);
    checkAllDescribed (slowSync);

    // Held in named variables: passing the temporary straight into find()
    // would leave the returned pointer dangling.
    const auto* slowEighth = find (slowSync, "1/8");
    const auto* fastEighth = find (fastSync, "1/8");

    TMIX_REQUIRE (slowEighth != nullptr);
    TMIX_REQUIRE (fastEighth != nullptr);

    TMIX_CHECK_NEAR (slowEighth->ms, 60000.0 / 90.0 / 2.0, 1e-6);
    TMIX_CHECK_NEAR (fastEighth->ms, 60000.0 / 140.0 / 2.0, 1e-6);
    TMIX_CHECK (slowEighth->ms > fastEighth->ms);

    // Past a quarter note it is no longer an effect.
    for (const auto& option : slowSync)
        TMIX_CHECK (option.ms <= slow.beatMs() + 1e-9);
}

// The decay table runs from a short room to a long hall, in order, and every
// row says what it is for.
TMIX_TEST (MixTiming_ReverbDecaysAreOrderedAndDescribed)
{
    const tmix::MixTiming timing (120.0);
    const auto rows = timing.reverbDecays();

    TMIX_CHECK (rows.size() >= 4);

    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        TMIX_CHECK (! rows[i].label.empty());
        TMIX_CHECK (! rows[i].use.empty());
        TMIX_CHECK (rows[i].ms > 0.0);

        if (i > 0)
            TMIX_CHECK (rows[i].ms > rows[i - 1].ms);
    }
}

// Pre-delay needs enough resolution to be useful, and it has to cover both
// sides of the Haas threshold: below it the reverb glues to the source, above
// it the reflection is heard separately. Both are legitimate choices, so the
// table spans them and says which is which.
TMIX_TEST (MixTiming_PreDelaySpansTheHaasThreshold)
{
    const tmix::MixTiming timing (140.0);
    const auto values = timing.preDelays();

    TMIX_CHECK (values.size() >= 10);
    checkAllDescribed (values);

    bool belowThreshold = false;
    bool aboveThreshold = false;

    for (const auto& value : values)
    {
        if (value.ms < timing.haasThresholdMs())
            belowThreshold = true;
        if (value.ms > timing.haasThresholdMs())
            aboveThreshold = true;
    }

    TMIX_CHECK (belowThreshold);
    TMIX_CHECK (aboveThreshold);

    // Ordered, and starting from none at all.
    TMIX_CHECK_NEAR (values.front().ms, 0.0, 1e-9);
    for (std::size_t i = 1; i < values.size(); ++i)
        TMIX_CHECK (values[i].ms > values[i - 1].ms);
}

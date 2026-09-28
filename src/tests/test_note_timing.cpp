#include "TestHarness.h"

#include "NoteTiming.h"

#include <stdexcept>
#include <string>

namespace {

const tmix::NoteValue* findLabel (const std::vector<tmix::NoteValue>& table,
                                  const std::string& label)
{
    for (const auto& entry : table)
        if (entry.label == label)
            return &entry;
    return nullptr;
}

} // namespace

// A quarter note at 120 BPM lasts exactly half a second. This is the
// arithmetic every other row of the table is derived from.
TMIX_TEST (NoteTiming_QuarterNoteAt120BpmIs500Ms)
{
    const tmix::NoteTiming timing (120.0);

    TMIX_CHECK_NEAR (timing.quarterNoteMs(), 500.0, 1e-9);
}

// A tempo of zero or below is meaningless and must not silently produce
// infinities that would propagate into every row of the table.
TMIX_TEST (NoteTiming_RejectsNonPositiveBpm)
{
    bool zeroThrew = false;
    try { const tmix::NoteTiming timing (0.0); }
    catch (const std::invalid_argument&) { zeroThrew = true; }
    TMIX_CHECK (zeroThrew);

    bool negativeThrew = false;
    try { const tmix::NoteTiming timing (-120.0); }
    catch (const std::invalid_argument&) { negativeThrew = true; }
    TMIX_CHECK (negativeThrew);
}

// Durations scale linearly with the note length in quarter-note beats.
TMIX_TEST (NoteTiming_MsScalesWithQuarterBeats)
{
    const tmix::NoteTiming timing (120.0);

    TMIX_CHECK_NEAR (timing.msForQuarterBeats (2.0), 1000.0, 1e-9); // half note
    TMIX_CHECK_NEAR (timing.msForQuarterBeats (0.5), 250.0, 1e-9);  // eighth note
}

// Six base values, each in plain / dotted / triplet form.
TMIX_TEST (NoteTiming_TableCoversPlainDottedAndTripletVariants)
{
    const tmix::NoteTiming timing (120.0);
    const auto& table = timing.table();

    const char* bases[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };

    for (const char* base : bases)
    {
        const std::string plain (base);
        TMIX_CHECK (findLabel (table, plain) != nullptr);
        TMIX_CHECK (findLabel (table, plain + "D") != nullptr);
        TMIX_CHECK (findLabel (table, plain + "T") != nullptr);
    }

    TMIX_CHECK_EQ (static_cast<int> (table.size()), 18);
}

// Known values at 120 BPM, hand-computed from a 500 ms quarter note.
TMIX_TEST (NoteTiming_DottedAndTripletDurationsAt120Bpm)
{
    const tmix::NoteTiming timing (120.0);
    const auto& table = timing.table();

    const auto* dottedEighth = findLabel (table, "1/8D");
    TMIX_CHECK (dottedEighth != nullptr);
    if (dottedEighth != nullptr)
        TMIX_CHECK_NEAR (dottedEighth->ms, 375.0, 1e-9); // 250 * 1.5

    const auto* tripletQuarter = findLabel (table, "1/4T");
    TMIX_CHECK (tripletQuarter != nullptr);
    if (tripletQuarter != nullptr)
        TMIX_CHECK_NEAR (tripletQuarter->ms, 500.0 * 2.0 / 3.0, 1e-9);

    const auto* wholeNote = findLabel (table, "1/1");
    TMIX_CHECK (wholeNote != nullptr);
    if (wholeNote != nullptr)
        TMIX_CHECK_NEAR (wholeNote->ms, 2000.0, 1e-9);
}

// The Hz column is what an LFO or filter sweep is set to, and is simply the
// reciprocal of the millisecond value.
TMIX_TEST (NoteTiming_HertzIsReciprocalOfMilliseconds)
{
    const tmix::NoteTiming timing (120.0);

    // Guard against the loop below passing vacuously over an empty table.
    TMIX_CHECK_EQ (static_cast<int> (timing.table().size()), 18);

    for (const auto& entry : timing.table())
    {
        TMIX_CHECK_NEAR (entry.hz, 1000.0 / entry.ms, 1e-6);
    }
}

// A dotted triplet is 1.5 * 2/3 = 1.0 of the base value. It is deliberately
// NOT a separate table row - it would duplicate the plain row and read as a
// bug - but the identity must hold for anyone computing it.
TMIX_TEST (NoteTiming_DottedTripletEqualsBaseDuration)
{
    const tmix::NoteTiming timing (120.0);

    const double base     = timing.msForQuarterBeats (1.0);
    const double dotted   = timing.msForQuarterBeats (1.0 * 1.5);
    const double triplet  = timing.msForQuarterBeats (1.0 * 2.0 / 3.0);
    const double dottedTriplet = timing.msForQuarterBeats (1.0 * 1.5 * 2.0 / 3.0);

    TMIX_CHECK_NEAR (dottedTriplet, base, 1e-9);
    TMIX_CHECK (dotted > base);
    TMIX_CHECK (triplet < base);
}

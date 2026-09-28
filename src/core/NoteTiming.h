#pragma once

#include <string>
#include <vector>

namespace tmix {

// One row of the note-value table: a note length expressed in quarter-note
// beats, its wall-clock duration at the current tempo, and the equivalent
// frequency for LFO-sync use.
struct NoteValue
{
    std::string label;        // "1/4", "1/8D" (dotted), "1/8T" (triplet)
    double      quarterBeats; // length in quarter-note beats
    double      ms;           // duration in milliseconds
    double      hz;           // 1000 / ms
};

// Converts a tempo into the note-value table the mixer works from. The table
// is rebuilt whenever the tempo changes, so a manual BPM correction simply
// constructs a new instance.
class NoteTiming
{
public:
    // Throws std::invalid_argument if bpm is not finite or not positive.
    explicit NoteTiming (double bpm);

    double bpm() const noexcept { return bpm_; }
    double quarterNoteMs() const noexcept { return quarterNoteMs_; }

    // Duration of a note whose length is given in quarter-note beats.
    double msForQuarterBeats (double quarterBeats) const noexcept;

    const std::vector<NoteValue>& table() const noexcept { return table_; }

private:
    double                bpm_           = 0.0;
    double                quarterNoteMs_ = 0.0;
    std::vector<NoteValue> table_;
};

} // namespace tmix

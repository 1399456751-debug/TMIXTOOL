#include "NoteTiming.h"

#include <cmath>
#include <stdexcept>

namespace tmix {

namespace {

struct BaseValue
{
    const char* label;
    double      quarterBeats;
};

// Note lengths expressed in quarter-note beats, longest first.
constexpr BaseValue kBaseValues[] = {
    { "1/1",  4.0   },
    { "1/2",  2.0   },
    { "1/4",  1.0   },
    { "1/8",  0.5   },
    { "1/16", 0.25  },
    { "1/32", 0.125 },
};

constexpr double kDottedMultiplier  = 1.5;
constexpr double kTripletMultiplier = 2.0 / 3.0;

} // namespace

NoteTiming::NoteTiming (double bpm)
    : bpm_ (bpm)
{
    if (! std::isfinite (bpm) || bpm <= 0.0)
        throw std::invalid_argument ("NoteTiming: BPM must be finite and positive");

    quarterNoteMs_ = 60000.0 / bpm_;

    table_.reserve (3 * (sizeof (kBaseValues) / sizeof (kBaseValues[0])));

    for (const auto& base : kBaseValues)
    {
        const std::string label (base.label);

        // Dotted triplets are intentionally absent: 1.5 * 2/3 == 1.0, so they
        // would duplicate the plain row and read as a bug.
        table_.push_back ({ label,       base.quarterBeats,                                       0.0, 0.0 });
        table_.push_back ({ label + "D", base.quarterBeats * kDottedMultiplier,                   0.0, 0.0 });
        table_.push_back ({ label + "T", base.quarterBeats * kTripletMultiplier,                  0.0, 0.0 });
    }

    for (auto& entry : table_)
    {
        entry.ms = msForQuarterBeats (entry.quarterBeats);
        entry.hz = 1000.0 / entry.ms;
    }
}

double NoteTiming::msForQuarterBeats (double quarterBeats) const noexcept
{
    return quarterBeats * quarterNoteMs_;
}

} // namespace tmix

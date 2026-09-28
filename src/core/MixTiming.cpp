#include "MixTiming.h"

#include <cmath>
#include <stdexcept>

namespace tmix {

namespace {

constexpr double kDotted = 1.5;

double beatsFor (double quarterBeats, bool dotted)
{
    return dotted ? quarterBeats * kDotted : quarterBeats;
}

TimeValue value (const char* label, const char* use, double ms)
{
    return { label, use, ms, ms > 0.0 ? 1000.0 / ms : 0.0 };
}

} // namespace

MixTiming::MixTiming (double bpm)
    : bpm_ (bpm)
{
    if (! std::isfinite (bpm) || bpm <= 0.0)
        throw std::invalid_argument ("MixTiming: BPM must be finite and positive");

    beatMs_ = 60000.0 / bpm_;
}

std::vector<DelayPreset> MixTiming::delayPresets() const
{
    struct Pairing
    {
        const char* name;
        double      leftBeats;
        double      rightBeats;
    };

    const Pairing pairings[] = {
        { "1/4 + 1/4",   beatsFor (1.0, false), beatsFor (1.0, false) },
        { "1/8D + 1/8D", beatsFor (0.5, true),  beatsFor (0.5, true)  },
        { "1/8 + 1/16",  beatsFor (0.5, false), beatsFor (0.25, false) },
        { "1/4D + 1/4",  beatsFor (1.0, true),  beatsFor (1.0, false) },
        { "1/2 + 1/4",   beatsFor (2.0, false), beatsFor (1.0, false) },
    };

    std::vector<DelayPreset> presets;
    for (const auto& pairing : pairings)
    {
        DelayPreset preset;
        preset.name    = pairing.name;
        preset.leftMs  = pairing.leftBeats * beatMs_;
        preset.rightMs = pairing.rightBeats * beatMs_;
        preset.leftHz  = 1000.0 / preset.leftMs;
        preset.rightHz = 1000.0 / preset.rightMs;
        presets.push_back (preset);
    }

    return presets;
}

std::vector<TimeValue> MixTiming::attackSuggestions() const
{
    const struct { const char* label; const char* use; double ms; } rows[] = {
        { "0.1 ms", "Brickwall - clip the peak, no movement"     , 0.1  },
        { "0.3 ms", "Catch the very front of a transient"        , 0.3  },
        { "1 ms",   "Kick and bass punch without losing the click", 1.0  },
        { "3 ms",   "Snare and drum bus - lets the hit through"  , 3.0  },
        { "10 ms",  "Vocal control - softens peaks, stays open"  , 10.0 },
        { "20 ms",  "Bus glue - rides the level, not the note"   , 20.0 },
        { "30 ms",  "Gentle levelling, transient passes through"  , 30.0 },
    };

    std::vector<TimeValue> values;
    for (const auto& row : rows)
        values.push_back (value (row.label, row.use, row.ms));

    return values;
}

std::vector<TimeValue> MixTiming::releaseSuggestions() const
{
    // Practical millisecond values. A quarter note at 90 BPM is 667 ms, which
    // no one dials in on purpose; these are the values that actually get used.
    const struct { const char* label; const char* use; double ms; } rows[] = {
        { "20 ms",  "Limiting - level returns before the next hit",  20.0  },
        { "50 ms",  "Drums - punchy, still reads as fast",           50.0  },
        { "80 ms",  "Drum bus - the usual starting point",           80.0  },
        { "120 ms", "General purpose",                              120.0  },
        { "180 ms", "Vocals - smooths without pumping",             180.0  },
        { "250 ms", "Program-dependent territory for most plugins", 250.0  },
        { "350 ms", "Bus glue - slow, lets transients breathe",     350.0  },
        { "500 ms", "Very slow levelling on a full mix",            500.0  },
    };

    std::vector<TimeValue> values;
    for (const auto& row : rows)
        values.push_back (value (row.label, row.use, row.ms));

    return values;
}

std::vector<TimeValue> MixTiming::releaseSyncedOptions() const
{
    // The deliberate pumping effect. Short note values only - past a quarter
    // note it stops being an effect.
    const struct { const char* label; const char* use; double beats; } rows[] = {
        { "1/16", "Tight rhythmic pumping"          , 0.25 },
        { "1/8",  "Classic pumping release"         , 0.5  },
        { "1/8D", "Swing feel against a straight beat", 0.5 * kDotted },
        { "1/4",  "Slow breathing on pads and keys" , 1.0  },
    };

    std::vector<TimeValue> values;
    for (const auto& row : rows)
        values.push_back (value (row.label, row.use, row.beats * beatMs_));

    return values;
}

std::vector<ReverbRow> MixTiming::reverbDecays() const
{
    const struct { const char* label; double beats; const char* use; } rows[] = {
        { "1/8",   0.5, "Very small room - air, no tail"        },
        { "1/4",   1.0, "Tight plate - drums keep their punch"  },
        { "1/2",   2.0, "General purpose - vocals and drums"    },
        { "1 bar", 4.0, "Hall - lets a phrase ring out"         },
        { "2 bars", 8.0, "Ambience - pads and sparse arrangements" },
    };

    std::vector<ReverbRow> values;
    for (const auto& row : rows)
        values.push_back ({ row.label, row.use, row.beats * beatMs_ });

    return values;
}

std::vector<TimeValue> MixTiming::preDelays() const
{
    // A full sweep from glued to the source up to a distinct slap. The Haas
    // threshold is called out at its value rather than used as a ceiling -
    // past it the result is a different effect, not a broken one.
    const struct { const char* label; const char* use; double ms; } rows[] = {
        { "0 ms",  "Glued - reverb starts with the source"       , 0.0  },
        { "5 ms",  "Barely separated, adds size only"            , 5.0  },
        { "10 ms", "Tight - keeps the source upfront"            , 10.0 },
        { "15 ms", "Small room"                                  , 15.0 },
        { "20 ms", "Vocal clarity - the usual starting point"     , 20.0 },
        { "25 ms", "Clear separation, still reads as one sound"  , 25.0 },
        { "30 ms", "At the Haas limit - about to split in two"   , 30.0 },
        { "40 ms", "Slap begins - heard as a separate reflection", 40.0 },
        { "50 ms", "Wide - deliberate slapback"                  , 50.0 },
        { "60 ms", "Large hall, source sits well forward"        , 60.0 },
        { "80 ms", "Very wide - almost an echo"                  , 80.0 },
        { "100 ms", "Echo territory, no longer a pre-delay"      , 100.0 },
    };

    std::vector<TimeValue> values;
    for (const auto& row : rows)
        values.push_back (value (row.label, row.use, row.ms));

    return values;
}

} // namespace tmix

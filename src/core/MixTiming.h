#pragma once

#include <string>
#include <vector>

namespace tmix {

// One labelled time value, with the frequency an LFO or filter sweep would be
// set to for the same duration, and a short note on when it is the right
// choice. That note is the point: a bare table of milliseconds does not help
// anyone decide.
struct TimeValue
{
    std::string label;
    std::string use;
    double      ms = 0.0;
    double      hz = 0.0;
};

// A stereo delay setting: the two sides usually carry different note values,
// which is what makes a delay sit behind the beat instead of on it.
struct DelayPreset
{
    std::string name;
    double      leftMs  = 0.0;
    double      rightMs = 0.0;
    double      leftHz  = 0.0;
    double      rightHz = 0.0;
};

// A reverb decay time with a plain-language note about where it tends to be
// used.
struct ReverbRow
{
    std::string label;
    std::string use;
    double      ms = 0.0;
};

// Everything the old bench-time tool produced, derived from one tempo.
class MixTiming
{
public:
    // Throws std::invalid_argument if bpm is not finite and positive.
    explicit MixTiming (double bpm);

    double bpm() const noexcept { return bpm_; }
    double beatMs() const noexcept { return beatMs_; }

    std::vector<DelayPreset> delayPresets() const;

    // Compressor attack.
    //
    // Fixed millisecond values, deliberately not tied to the tempo. An attack
    // is about catching or passing a transient; expressing it as a note length
    // would make it slip at slow tempos and stop doing its job.
    std::vector<TimeValue> attackSuggestions() const;

    // Compressor release, as practical millisecond values.
    //
    // Also not note-derived. A quarter note at 90 BPM is 667 ms, which is a
    // release nobody would dial in - useful releases live between roughly 20
    // and 500 ms regardless of tempo. Note-synced releases are a separate,
    // narrower technique and get their own list.
    std::vector<TimeValue> releaseSuggestions() const;

    // Beat-synced release, for the deliberate pumping effect. Short note
    // values only: past a quarter note it stops being an effect and becomes a
    // level-control setting that happens to be in time.
    std::vector<TimeValue> releaseSyncedOptions() const;

    // Reverb decay times, shortest first.
    std::vector<ReverbRow> reverbDecays() const;

    // Pre-delay, from glued to the source through to a distinct slap. Values
    // beyond the Haas threshold are legitimate but sound like a separate
    // reflection rather than part of the reverb, and the table says so.
    std::vector<TimeValue> preDelays() const;

    double haasThresholdMs() const noexcept { return 30.0; }

private:
    double bpm_    = 0.0;
    double beatMs_ = 0.0;
};

} // namespace tmix

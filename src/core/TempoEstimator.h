#pragma once

#include "OnsetDetector.h"

#include <vector>

namespace tmix {

// One tempo reading offered to the user. The 1/2 and 2x relatives are always
// reported rather than hidden, because half/double confusion is the single
// most common way a tempo detector misleads a mixing engineer - and because
// the user is the final judge of which reading matches the material.
struct TempoCandidate
{
    double bpm           = 0.0;
    double score         = 0.0; // 0..1, relative within one result
    bool   halfOfPrimary = false;
    bool   doubleOfPrimary = false;
};

struct TempoResult
{
    bool   valid    = false;
    double bpm      = 0.0;  // reported estimate, after octave folding
    double rawBpm   = 0.0;  // what the search produced, before folding
    bool   folded   = false; // true when the octave was changed
    double periodSeconds = 0.0;
    double confidence = 0.0; // 0..1
    bool   uncertain  = false; // true when confidence is below the threshold

    // Phase of the beat grid: the time of a beat, not necessarily a downbeat.
    double firstBeatSeconds = 0.0;

    // How tightly the detected events sit on the grid, 0..1. Reported because
    // it is the single most informative confidence signal.
    double phaseConcentration = 0.0;

    std::vector<TempoCandidate> candidates; // ranked, best first

    // Per-candidate family scores used to decide the half/double reading.
    double lowBandAlignment  = 0.0;
    double highBandAlignment = 0.0;
};

struct TempoConfig
{
    double minBpm = 60.0;
    double maxBpm = 200.0;

    // Soft prior over tempo. Pulls the answer toward the centre only when the
    // evidence is close; it never overrides strong evidence.
    double priorCentreBpm    = 120.0;
    double priorSigmaOctaves = 0.9;

    double confidenceThreshold = 0.45;

    // The octave the answer is snapped into.
    //
    // Half and double tempo are genuinely ambiguous for a lot of material: an
    // evenly spaced pulse train is exactly as consistent with twice its rate,
    // because an impulse train contains every harmonic. No amount of tuning
    // resolves that reliably - it is a property of the signal, not a bug.
    //
    // Rather than pretend otherwise, the search stays wide and the answer is
    // folded by octaves into a range the user declares their material lives
    // in. That is predictable, it is under their control, and it is honest
    // about what is actually being decided.
    double preferredMinBpm = 70.0;
    double preferredMaxBpm = 150.0;
};

// Estimates tempo, beat phase and a confidence from an onset envelope.
TempoResult estimateTempo (const OnsetEnvelope& envelope,
                           const TempoConfig& config = {});

} // namespace tmix

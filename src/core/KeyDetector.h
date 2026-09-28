#pragma once

#include <string>
#include <vector>

namespace tmix {

struct KeyCandidate
{
    std::string name;  // "C major", "A minor"
    int    rootPitchClass = 0; // 0 == C
    bool   isMajor = true;
    double score   = 0.0;      // 0..1, relative within one result
};

struct KeyResult
{
    bool        valid = false;
    std::string name;
    int         rootPitchClass = 0;
    bool        isMajor = true;

    double confidence = 0.0; // 0..1
    bool   uncertain  = false;

    // Estimated reference pitch, reported as the frequency of A4.
    //
    // Material is not always at concert pitch: tape transfers drift, older
    // records were cut to whatever the studio's tuning fork said, and some
    // productions are deliberately detuned. Worth knowing on its own - if a
    // track sits at A=441, that is a fact about the source, not a detection
    // error to be hidden.
    bool   tuningValid = false;
    double referenceHz = 440.0;
    double tuningCents = 0.0; // deviation from 440, positive means sharp

    // Ranked, best first. Relative major and minor share six of their seven
    // notes, so the runner-up is often worth showing rather than hiding.
    std::vector<KeyCandidate> candidates;
};

struct KeyConfig
{
    int    fftSize  = 4096;
    int    hopSize  = 2048;
    double minHz    = 55.0;   // A1
    double maxHz    = 2000.0; // B6

    // A second chroma is built from the bass region only and blended in. The
    // full-range chroma discards octave information, and without it a bare
    // triad is genuinely ambiguous: C-E-G correlates just as well with E minor
    // as with C major, because the minor profile weights its own triad tones
    // more heavily. The bass note is what tells the two apart - C major has C
    // underneath, E minor has E.
    double bassMaxHz  = 250.0;
    double bassWeight = 0.35;

    double confidenceThreshold = 0.35;
};

// Estimates the key of a monophonic-summed signal.
//
// The chroma is built from a windowed spectrum, aggregated across frames, and
// correlated against the Krumhansl-Kessler key profiles. Relative major and
// minor are genuinely hard to separate - they share almost all their notes -
// so the result reports the runner-up and lowers its confidence instead of
// pretending to be certain.
KeyResult detectKey (const std::vector<float>& mono,
                     int sampleRate,
                     const KeyConfig& config = {});

} // namespace tmix

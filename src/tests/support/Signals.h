#pragma once

// Test-only synthetic audio fixtures with known ground truth.
//
// Everything here is a pure function of its arguments and uses a fixed
// recurrence instead of <random>, so a fixture is byte-identical on every run
// and across standard library versions. Without that, a failure could not be
// reproduced and a golden baseline would be meaningless.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tmixsupport {

constexpr double kPi = 3.14159265358979323846;

enum class Drum
{
    Kick,
    Snare,
    Hat
};

struct Hit
{
    double beat; // position in quarter-note beats from the start
    Drum   drum;
    double amplitude;
};

// Deterministic noise source (linear congruential, as in the FFT fixture).
class Noise
{
public:
    explicit Noise (std::uint32_t seed) : state_ (seed) {}

    double next() // in [-1, 1)
    {
        state_ = state_ * 1664525u + 1013904223u;
        return static_cast<double> (state_ >> 8) / static_cast<double> (1u << 23) - 1.0;
    }

private:
    std::uint32_t state_;
};

inline std::size_t sampleIndex (double seconds, int sampleRate, std::size_t limit)
{
    if (seconds < 0.0)
        return 0;

    const double index = seconds * static_cast<double> (sampleRate);
    if (index >= static_cast<double> (limit))
        return limit;

    return static_cast<std::size_t> (index);
}

// Pitch-swept sine with an exponential tail - the low-frequency, sharply
// attacked transient that tempo detectors key on.
inline void addKick (std::vector<float>& out, int sampleRate, double atSeconds,
                     double amplitude)
{
    const std::size_t start = sampleIndex (atSeconds, sampleRate, out.size());
    if (start >= out.size())
        return;

    double phase = 0.0;
    for (std::size_t i = start; i < out.size(); ++i)
    {
        const double t = static_cast<double> (i - start) / sampleRate;
        if (t > 0.5)
            break;

        const double frequency = 55.0 + 90.0 * std::exp (-t / 0.02);
        phase += 2.0 * kPi * frequency / sampleRate;

        const double envelope = std::exp (-t / 0.09);
        out[i] += static_cast<float> (amplitude * envelope * std::sin (phase));
    }
}

// Noise burst with a tonal body: broadband, mid-band energy.
inline void addSnare (std::vector<float>& out, int sampleRate, double atSeconds,
                      double amplitude, Noise& noise)
{
    const std::size_t start = sampleIndex (atSeconds, sampleRate, out.size());
    if (start >= out.size())
        return;

    double phase = 0.0;
    for (std::size_t i = start; i < out.size(); ++i)
    {
        const double t = static_cast<double> (i - start) / sampleRate;
        if (t > 0.3)
            break;

        const double envelope = std::exp (-t / 0.05);
        phase += 2.0 * kPi * 190.0 / sampleRate;

        const double body  = 0.35 * std::sin (phase);
        const double value = amplitude * envelope * (body + 0.85 * noise.next());
        out[i] += static_cast<float> (value);
    }
}

// Very short high-frequency burst. The first difference stands in for a
// high-pass, which is enough to place its energy well above the kick.
inline void addHat (std::vector<float>& out, int sampleRate, double atSeconds,
                    double amplitude, Noise& noise)
{
    const std::size_t start = sampleIndex (atSeconds, sampleRate, out.size());
    if (start >= out.size())
        return;

    double previous = 0.0;
    for (std::size_t i = start; i < out.size(); ++i)
    {
        const double t = static_cast<double> (i - start) / sampleRate;
        if (t > 0.12)
            break;

        const double envelope = std::exp (-t / 0.012);
        const double raw      = noise.next();
        const double highPassed = raw - previous;
        previous = raw;

        out[i] += static_cast<float> (amplitude * envelope * highPassed);
    }
}

inline void addDrum (std::vector<float>& out, int sampleRate, double atSeconds,
                     Drum drum, double amplitude, Noise& noise)
{
    switch (drum)
    {
        case Drum::Kick:  addKick  (out, sampleRate, atSeconds, amplitude);        break;
        case Drum::Snare: addSnare (out, sampleRate, atSeconds, amplitude, noise); break;
        case Drum::Hat:   addHat   (out, sampleRate, atSeconds, amplitude, noise); break;
    }
}

// Renders a hit list at a known tempo. `hits` positions are in quarter-note
// beats, so beat 4 is exactly one bar into a 4/4 pattern.
inline std::vector<float> renderPattern (const std::vector<Hit>& hits,
                                         double bpm,
                                         int sampleRate,
                                         double seconds)
{
    const std::size_t frameCount =
        static_cast<std::size_t> (seconds * static_cast<double> (sampleRate));

    std::vector<float> out (frameCount, 0.0f);
    Noise noise (20260927u);

    const double secondsPerBeat = 60.0 / bpm;

    for (const auto& hit : hits)
        addDrum (out, sampleRate, hit.beat * secondsPerBeat, hit.drum, hit.amplitude, noise);

    return out;
}

// Builds a repeating pattern over the requested duration.
inline std::vector<Hit> repeatPattern (const std::vector<Hit>& bar,
                                       double bars)
{
    std::vector<Hit> hits;

    double maxBeat = 0.0;
    for (const auto& hit : bar)
        maxBeat = std::max (maxBeat, hit.beat);

    const double barLength = maxBeat + 1.0; // assume the pattern ends on a beat

    for (int b = 0; b < static_cast<int> (bars); ++b)
        for (const auto& hit : bar)
            hits.push_back ({ hit.beat + b * barLength, hit.drum, hit.amplitude });

    return hits;
}

// --- Chord fixtures, used by the key tests --------------------------------

inline double midiToHz (int midiNote)
{
    return 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);
}

// A sustained chord. Each note carries a couple of harmonics so the spectrum
// resembles an instrument: a bank of pure sines has no harmonic structure and
// would not exercise the pitch-class mapping the way real material does.
//
// detuneCents shifts every note together, which is what a recording made at a
// non-standard reference pitch looks like.
inline std::vector<float> chord (const std::vector<int>& midiNotes,
                                 int sampleRate,
                                 double seconds,
                                 double detuneCents = 0.0,
                                 bool perNoteDrift = true)
{
    const auto frames = static_cast<std::size_t> (seconds * sampleRate);
    std::vector<float> out (frames, 0.0f);

    const double detune = std::pow (2.0, detuneCents / 1200.0);

    for (const int note : midiNotes)
    {
        const double fundamental = midiToHz (note) * detune;

        // Slight detune per note keeps the sum from being a single periodic
        // waveform with an artificially clean spectrum. Switched off for the
        // reference-pitch tests: a chord whose notes are each offset by a
        // different amount has no single reference pitch to measure, so
        // expecting one back would be a badly posed question.
        const double drift = perNoteDrift ? (1.0 + 0.0007 * ((note % 5) - 2)) : 1.0;

        for (std::size_t i = 0; i < frames; ++i)
        {
            const double t = static_cast<double> (i) / sampleRate;

            double value = 0.0;
            value += std::sin (2.0 * kPi * fundamental * drift * t);
            value += 0.5 * std::sin (2.0 * kPi * fundamental * drift * 2.0 * t);
            value += 0.25 * std::sin (2.0 * kPi * fundamental * drift * 3.0 * t);

            out[i] += static_cast<float> (value);
        }
    }

    // Normalise so chords of different sizes are comparable.
    float peak = 0.0f;
    for (const float value : out)
        peak = std::max (peak, std::fabs (value));

    if (peak > 0.0f)
        for (auto& value : out)
            value *= 0.4f / peak;

    return out;
}

// --- Named fixtures used by the tempo tests -------------------------------

// Kick on every beat: the simplest unambiguous tempo reference.
inline std::vector<float> clickTrack (double bpm, int sampleRate, double seconds)
{
    std::vector<Hit> hits;
    const int beats = static_cast<int> (seconds * bpm / 60.0);
    for (int b = 0; b < beats; ++b)
        hits.push_back ({ static_cast<double> (b), Drum::Kick, 0.9 });

    return renderPattern (hits, bpm, sampleRate, seconds);
}

// Kick on 1 and 3, snare on 2 and 4, hats on every eighth.
inline std::vector<float> straightRock (double bpm, int sampleRate, double seconds)
{
    const std::vector<Hit> bar = {
        { 0.0, Drum::Kick,  0.95 },
        { 1.0, Drum::Snare, 0.80 },
        { 2.0, Drum::Kick,  0.90 },
        { 3.0, Drum::Snare, 0.80 },
        { 0.5, Drum::Hat,   0.35 },
        { 1.5, Drum::Hat,   0.35 },
        { 2.5, Drum::Hat,   0.35 },
        { 3.5, Drum::Hat,   0.35 },
    };

    const double bars = seconds * bpm / (60.0 * 4.0);
    return renderPattern (repeatPattern (bar, bars), bpm, sampleRate, seconds);
}

// Kick on 1, snare on 3: the classic half-time feel, where the snare backbeat
// sits at half the rate of the underlying pulse. A detector that latches onto
// the snare will report half the true tempo.
inline std::vector<float> halfTimeFeel (double bpm, int sampleRate, double seconds)
{
    const std::vector<Hit> bar = {
        { 0.0, Drum::Kick,  0.95 },
        { 2.0, Drum::Snare, 0.85 },
    };

    const double bars = seconds * bpm / (60.0 * 4.0);
    return renderPattern (repeatPattern (bar, bars), bpm, sampleRate, seconds);
}

// Kick on every beat with a louder accent every fourth beat. Tests that the
// accent does not drag the estimate down to a quarter of the real tempo.
inline std::vector<float> accentedFour (double bpm, int sampleRate, double seconds)
{
    std::vector<Hit> hits;
    const int beats = static_cast<int> (seconds * bpm / 60.0);

    for (int b = 0; b < beats; ++b)
        hits.push_back ({ static_cast<double> (b), Drum::Kick,
                          (b % 4 == 0) ? 1.0 : 0.6 });

    return renderPattern (hits, bpm, sampleRate, seconds);
}

// Four-on-the-floor with offbeat hats: the classic dance pattern. Every beat
// carries a kick and the hats fill the offbeats, so the densest regular event
// stream - and the one an autocorrelation locks onto first - runs at twice the
// beat rate.
inline std::vector<float> fourOnTheFloor (double bpm, int sampleRate, double seconds)
{
    std::vector<Hit> bar;

    for (int beat = 0; beat < 4; ++beat)
    {
        bar.push_back ({ static_cast<double> (beat), Drum::Kick, 0.95 });
        bar.push_back ({ static_cast<double> (beat) + 0.5, Drum::Hat, 0.55 });
    }

    const double bars = seconds * bpm / (60.0 * 4.0);
    return renderPattern (repeatPattern (bar, bars), bpm, sampleRate, seconds);
}

// Kick on every beat, accented every other beat, with hats on every sixteenth.
//
// The dense subdivision is the point. It gives a grid at three halves of the
// true tempo (and one at twice it) plenty of real onsets to land on, without
// either of them being the rate a listener counts.
inline std::vector<float> sixteenthHats (double bpm, int sampleRate, double seconds)
{
    std::vector<Hit> bar;

    for (int beat = 0; beat < 4; ++beat)
    {
        bar.push_back ({ static_cast<double> (beat), Drum::Kick,
                         (beat % 2 == 0) ? 0.95 : 0.55 });

        for (int sixteenth = 0; sixteenth < 4; ++sixteenth)
            bar.push_back ({ static_cast<double> (beat) + 0.25 * sixteenth,
                             Drum::Hat, 0.5 });
    }

    const double bars = seconds * bpm / (60.0 * 4.0);
    return renderPattern (repeatPattern (bar, bars), bpm, sampleRate, seconds);
}

} // namespace tmixsupport

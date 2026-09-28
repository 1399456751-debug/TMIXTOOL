#include "TestHarness.h"

#include "support/Signals.h"

#include "Decimator.h"
#include "OnsetDetector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

namespace {

constexpr int kSourceRate = 44100;

// Runs the same path the pipeline takes: decimate to the analysis rate, then
// compute the onset envelope.
tmix::OnsetEnvelope analyse (const std::vector<float>& signal)
{
    const tmix::Decimator decimator (kSourceRate, 22050);
    const auto analysis = decimator.process (signal);
    return tmix::computeOnsetEnvelope (analysis, decimator.outputRate());
}

double median (std::vector<float> values)
{
    if (values.empty())
        return 0.0;

    const std::size_t middle = values.size() / 2;
    std::nth_element (values.begin(), values.begin() + static_cast<std::ptrdiff_t> (middle),
                      values.end());
    return static_cast<double> (values[middle]);
}

// Largest envelope value inside a time window.
double maxInWindow (const tmix::OnsetEnvelope& envelope, double fromSeconds, double toSeconds)
{
    double best = 0.0;
    for (std::size_t f = 0; f < envelope.frameCount(); ++f)
    {
        const double t = envelope.frameTime (f);
        if (t >= fromSeconds && t <= toSeconds)
            best = std::max (best, static_cast<double> (envelope.full[f]));
    }
    return best;
}

} // namespace

TMIX_TEST (Onset_SilenceProducesNoStrength)
{
    const std::vector<float> silence (static_cast<std::size_t> (kSourceRate), 0.0f);

    const auto envelope = analyse (silence);

    TMIX_CHECK (envelope.frameCount() > 0);
    for (const float value : envelope.full)
        TMIX_CHECK_NEAR (value, 0.0, 1e-6);
}

// The frame rate follows from the hop size, and downstream tempo maths depends
// on it being reported correctly.
TMIX_TEST (Onset_ReportsFrameRateAndHop)
{
    const std::vector<float> signal (static_cast<std::size_t> (kSourceRate), 0.0f);
    const auto envelope = analyse (signal);

    const tmix::Decimator decimator (kSourceRate, 22050);
    const double expectedHop = 512.0 / static_cast<double> (decimator.outputRate());

    TMIX_CHECK_NEAR (envelope.hopSeconds, expectedHop, 1e-9);
    TMIX_CHECK_NEAR (envelope.frameRate, 1.0 / expectedHop, 1e-6);
}

// The core requirement: onset strength must peak on the beat, not between
// beats. A systematic timing error here would shift every reported beat
// position and put the click track out of alignment.
TMIX_TEST (Onset_PeaksOnTheBeatGrid)
{
    constexpr double bpm = 120.0;
    constexpr double leadInSeconds = 0.5;

    // Spectral flux is a difference between consecutive frames, so an onset
    // sitting in the very first frame has nothing to be measured against.
    // Real material is not cut to start exactly on a downbeat either; a short
    // lead-in is prepended rather than pretending the limitation is absent.
    const auto clicks = tmixsupport::clickTrack (bpm, kSourceRate, 8.0);

    std::vector<float> signal (static_cast<std::size_t> (leadInSeconds * kSourceRate), 0.0f);
    signal.insert (signal.end(), clicks.begin(), clicks.end());

    const auto envelope = analyse (signal);

    TMIX_REQUIRE (envelope.frameCount() > 0);

    const double baseline = median (envelope.full);
    TMIX_CHECK (baseline >= 0.0);

    const double secondsPerBeat = 60.0 / bpm;

    for (int beat = 0; beat < 16; ++beat)
    {
        const double beatTime = leadInSeconds + static_cast<double> (beat) * secondsPerBeat;

        const double onBeat  = maxInWindow (envelope,
                                            beatTime - 0.06,
                                            beatTime + 0.06);
        const double offBeat = maxInWindow (envelope,
                                            beatTime + 0.35 * secondsPerBeat,
                                            beatTime + 0.65 * secondsPerBeat);

        TMIX_CHECK (onBeat > 0.0);
        TMIX_CHECK (onBeat > baseline * 5.0);
        TMIX_CHECK (onBeat > offBeat * 3.0);
    }
}

// The band split is what lets the tempo stage tell a kick from a hat, so it
// has to actually separate the two.
TMIX_TEST (Onset_BandsSeparateKickFromHat)
{
    std::vector<tmixsupport::Hit> kicks;
    for (int b = 0; b < 16; ++b)
        kicks.push_back ({ static_cast<double> (b), tmixsupport::Drum::Kick, 0.9 });

    std::vector<tmixsupport::Hit> hats;
    for (int b = 0; b < 32; ++b)
        hats.push_back ({ static_cast<double> (b) * 0.5, tmixsupport::Drum::Hat, 0.7 });

    const auto kickSignal = tmixsupport::renderPattern (kicks, 120.0, kSourceRate, 8.0);
    const auto hatSignal  = tmixsupport::renderPattern (hats,  120.0, kSourceRate, 8.0);

    const auto kickEnvelope = analyse (kickSignal);
    const auto hatEnvelope  = analyse (hatSignal);

    // Shares rather than absolute sums. Log compression deliberately narrows
    // the gap between a loud kick and a quiet hat, so absolute band totals are
    // not comparable across signals - what the disambiguation stage needs to
    // know is how much of *this* signal's onset energy sits in each band.
    const double kickLow = std::accumulate (kickEnvelope.low.begin(),
                                            kickEnvelope.low.end(), 0.0);
    const double hatLow  = std::accumulate (hatEnvelope.low.begin(),
                                            hatEnvelope.low.end(), 0.0);
    const double kickHigh = std::accumulate (kickEnvelope.high.begin(),
                                             kickEnvelope.high.end(), 0.0);
    const double hatHigh  = std::accumulate (hatEnvelope.high.begin(),
                                             hatEnvelope.high.end(), 0.0);
    const double kickTotal = std::accumulate (kickEnvelope.full.begin(),
                                              kickEnvelope.full.end(), 0.0);
    const double hatTotal  = std::accumulate (hatEnvelope.full.begin(),
                                              hatEnvelope.full.end(), 0.0);

    TMIX_REQUIRE (kickTotal > 0.0);
    TMIX_REQUIRE (hatTotal > 0.0);

    const double kickLowShare  = kickLow / kickTotal;
    const double hatLowShare   = hatLow / hatTotal;
    const double kickHighShare = kickHigh / kickTotal;
    const double hatHighShare  = hatHigh / hatTotal;

    TMIX_CHECK (kickLowShare > hatLowShare * 3.0);

    // A kick's attack is abrupt enough to splatter across the whole spectrum,
    // so its high-band share stays substantial and "high share" alone cannot
    // separate the two. The discriminating quantity is the balance between the
    // bands: a kick is low-dominant, a hat is high-dominant.
    const double kickLowToHigh = kickLowShare / kickHighShare;
    const double hatLowToHigh  = hatLowShare / hatHighShare;

    TMIX_CHECK (kickLowToHigh > hatLowToHigh * 3.0);
}

// Where does the onset curve place a single transient? A constant offset here
// shifts every reported beat time, so it is measured rather than assumed.
TMIX_TEST (Onset_TransientLandsAtItsTrueTime)
{
    constexpr double hitBeat     = 1.0;   // one beat into a 60 BPM grid
    constexpr double hitSeconds  = 1.0;
    constexpr double bpm         = 60.0;

    const std::vector<tmixsupport::Hit> hits = {
        { hitBeat, tmixsupport::Drum::Kick, 1.0 }
    };

    const auto signal = tmixsupport::renderPattern (hits, bpm, kSourceRate, 3.0);
    const auto envelope = analyse (signal);

    TMIX_REQUIRE (envelope.frameCount() > 0);

    std::size_t peakFrame = 0;
    for (std::size_t f = 0; f < envelope.frameCount(); ++f)
        if (envelope.full[f] > envelope.full[peakFrame])
            peakFrame = f;

    const double peakTime = envelope.frameTime (peakFrame);

    std::printf ("      transient at %.4f s, onset peak at %.4f s, offset %+.4f s\n",
                 hitSeconds, peakTime, peakTime - hitSeconds);

    // Half a hop at the analysis rate; a looser bound would hide a real bias.
    TMIX_CHECK (std::fabs (peakTime - hitSeconds) < 0.012);
}

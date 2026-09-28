#include "KeyDetector.h"

#include "Fft.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace tmix {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Krumhansl-Kessler key profiles: how stable each scale degree is heard to be,
// measured with probe-tone experiments. Index 0 is the tonic.
constexpr double kMajorProfile[12] = {
    6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88
};

constexpr double kMinorProfile[12] = {
    6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17
};

const char* kNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

double meanOf (const std::array<double, 12>& values)
{
    double sum = 0.0;
    for (const double value : values)
        sum += value;
    return sum / 12.0;
}

// Pearson correlation between the chroma and a profile rotated so that `root`
// sits at index 0.
double correlate (const std::array<double, 12>& chroma,
                  const double* profile,
                  int root)
{
    const double chromaMean = meanOf (chroma);

    double profileMean = 0.0;
    for (int i = 0; i < 12; ++i)
        profileMean += profile[i];
    profileMean /= 12.0;

    double covariance = 0.0;
    double chromaEnergy = 0.0;
    double profileEnergy = 0.0;

    for (int i = 0; i < 12; ++i)
    {
        const double a = chroma[static_cast<std::size_t> ((i + root) % 12)] - chromaMean;
        const double b = profile[i] - profileMean;

        covariance    += a * b;
        chromaEnergy  += a * a;
        profileEnergy += b * b;
    }

    const double denominator = std::sqrt (chromaEnergy * profileEnergy);
    return denominator > 1e-12 ? covariance / denominator : 0.0;
}

} // namespace

KeyResult detectKey (const std::vector<float>& mono,
                     int sampleRate,
                     const KeyConfig& config)
{
    KeyResult result;

    const auto fftSize = static_cast<std::size_t> (config.fftSize);
    const auto hopSize = static_cast<std::size_t> (config.hopSize);

    if (sampleRate <= 0 || fftSize < 2 || hopSize == 0 || mono.size() < fftSize)
        return result;

    std::array<double, 12> chroma { };
    std::array<double, 12> bassChroma { };
    chroma.fill (0.0);
    bassChroma.fill (0.0);

    // One bin per cent, folded into a single octave. Only the reference-pitch
    // estimate uses this: twelve pitch classes cannot resolve the difference
    // between A=440 and A=441, which is four cents.
    constexpr int kFineBins = 1200;
    std::array<double, kFineBins> fineHistogram { };
    fineHistogram.fill (0.0);

    std::vector<float> window (fftSize);
    for (std::size_t i = 0; i < fftSize; ++i)
    {
        const double angle = 2.0 * kPi * static_cast<double> (i)
                             / static_cast<double> (fftSize - 1);
        window[i] = static_cast<float> (0.5 - 0.5 * std::cos (angle));
    }

    const Fft fft (fftSize);
    std::vector<std::complex<float>> spectrum (fftSize);
    const std::size_t bins = fftSize / 2 + 1;

    const double binHz = static_cast<double> (sampleRate) / static_cast<double> (fftSize);
    const int firstBin = std::max (1, static_cast<int> (config.minHz / binHz));
    const int lastBin  = std::min (static_cast<int> (bins) - 1,
                                   static_cast<int> (config.maxHz / binHz));

    std::size_t frames = 0;
    double totalEnergy = 0.0;

    for (std::size_t start = 0; start + fftSize <= mono.size(); start += hopSize)
    {
        for (std::size_t i = 0; i < fftSize; ++i)
            spectrum[i] = { mono[start + i] * window[i], 0.0f };

        fft.forward (spectrum.data());

        std::array<double, 12> frame { };
        std::array<double, 12> bassFrame { };
        std::array<double, kFineBins> fineFrame { };
        frame.fill (0.0);
        bassFrame.fill (0.0);
        fineFrame.fill (0.0);

        double frameEnergy = 0.0;

        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            const double magnitude = std::abs (spectrum[static_cast<std::size_t> (bin)]);
            if (magnitude <= 0.0)
                continue;

            frameEnergy += magnitude;

            const double hz = static_cast<double> (bin) * binHz;
            if (hz <= 0.0)
                continue;

            // Fractional MIDI note, then linear split across the two nearest
            // pitch classes. Splitting rather than rounding avoids the energy
            // jumping between classes as a note sits slightly off pitch.
            const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);

            const double withinOctave = midi - 12.0 * std::floor (midi / 12.0);
            const auto fineIndex = static_cast<int> (withinOctave * 100.0);
            if (fineIndex >= 0 && fineIndex < kFineBins)
                fineFrame[static_cast<std::size_t> (fineIndex)] += magnitude;

            const double floorMidi = std::floor (midi);
            const double fraction = midi - floorMidi;

            const auto lower = static_cast<int> (std::fmod (floorMidi, 12.0));
            const int lowerClass = (lower % 12 + 12) % 12;
            const int upperClass = (lowerClass + 1) % 12;

            frame[static_cast<std::size_t> (lowerClass)] += magnitude * (1.0 - fraction);
            frame[static_cast<std::size_t> (upperClass)] += magnitude * fraction;

            if (hz <= config.bassMaxHz)
            {
                bassFrame[static_cast<std::size_t> (lowerClass)] += magnitude * (1.0 - fraction);
                bassFrame[static_cast<std::size_t> (upperClass)] += magnitude * fraction;
            }
        }

        // Normalising each frame stops loud sections from deciding the key on
        // their own; a quiet verse is as informative as a loud chorus.
        if (frameEnergy > 1e-9)
        {
            for (auto& value : frame)
                value /= frameEnergy;

            std::array<double, 12> bassNormalised = bassFrame;
            double bassSum = 0.0;
            for (const double value : bassNormalised)
                bassSum += value;

            for (std::size_t i = 0; i < 12; ++i)
            {
                chroma[i] += frame[i];
                if (bassSum > 1e-9)
                    bassChroma[i] += bassNormalised[i] / bassSum;
            }

            double fineSum = 0.0;
            for (const double value : fineFrame)
                fineSum += value;

            if (fineSum > 1e-12)
                for (std::size_t i = 0; i < kFineBins; ++i)
                    fineHistogram[i] += fineFrame[i] / fineSum;

            ++frames;
        }

        totalEnergy += frameEnergy;
    }

    if (frames == 0 || totalEnergy <= 1e-9)
        return result; // Silence carries no key.

    // Blend the bass chroma in. Both are already per-frame normalised, so the
    // weighting is meaningful rather than tracking overall level.
    const double frameCount = static_cast<double> (frames);
    if (frameCount > 0.0)
    {
        for (std::size_t i = 0; i < 12; ++i)
        {
            chroma[i] /= frameCount;
            bassChroma[i] /= frameCount;
        }
    }

    std::array<double, 12> blended { };
    for (std::size_t i = 0; i < 12; ++i)
        blended[i] = (1.0 - config.bassWeight) * chroma[i]
                     + config.bassWeight * bassChroma[i];

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4996)
#endif
    const bool debug = std::getenv ("TMIX_DEBUG_KEY") != nullptr;
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

    if (debug)
    {
        std::printf ("      blended chroma:");
        for (std::size_t i = 0; i < 12; ++i)
            std::printf (" %s=%.3f", kNoteNames[i], blended[i]);
        std::printf ("\n");
    }

    std::vector<KeyCandidate> candidates;
    candidates.reserve (24);

    for (int root = 0; root < 12; ++root)
    {
        const double major = correlate (blended, kMajorProfile, root);
        const double minor = correlate (blended, kMinorProfile, root);

        candidates.push_back ({ std::string (kNoteNames[root]) + " major",
                                root, true, major });
        candidates.push_back ({ std::string (kNoteNames[root]) + " minor",
                                root, false, minor });
    }

    std::sort (candidates.begin(), candidates.end(),
               [] (const KeyCandidate& a, const KeyCandidate& b)
               { return a.score > b.score; });

    const KeyCandidate& best = candidates.front();
    const double second = candidates.size() > 1 ? candidates[1].score : 0.0;

    result.valid          = true;
    result.name           = best.name;
    result.rootPitchClass = best.rootPitchClass;
    result.isMajor        = best.isMajor;
    result.candidates     = candidates;

    // Confidence needs two things, not one.
    //
    // The margin over the runner-up says how clearly this key beats the next
    // one. But a flat chroma - noise, or a very dense mix - has no tonal
    // centre at all, and a profile can still fit it a little better than its
    // rivals by chance. So the margin is gated by how concentrated the chroma
    // actually is.
    const double margin = std::max (0.0, best.score - second);

    double chromaPeak = 0.0;
    double chromaSum  = 0.0;
    for (const double value : blended)
    {
        chromaPeak = std::max (chromaPeak, value);
        chromaSum += value;
    }

    const double chromaMean = chromaSum / 12.0;
    const double concentration = chromaPeak > 1e-12
        ? std::max (0.0, (chromaPeak - chromaMean) / chromaPeak)
        : 0.0;

    const double marginTerm = std::min (1.0, margin / 0.12);
    const double focusTerm  = std::min (1.0, concentration / 0.45);

    result.confidence = marginTerm * focusTerm;
    result.uncertain  = result.confidence < config.confidenceThreshold;

    // ---- reference pitch -------------------------------------------------
    //
    // Every spectral peak votes for how far it sits from the nearest
    // equal-tempered semitone. Notes that agree with each other cluster at one
    // deviation, and the centre of that cluster is the reference pitch.
    //
    // This replaced an earlier approach that searched for the offset making
    // the twelve pitch classes look sharpest. That curve turned out to be
    // almost flat near its peak - a few tenths of a percent between the best
    // and second best - so it landed anywhere within about six cents, which is
    // not enough to tell A=440 from A=441. Voting directly on the deviation is
    // both simpler and finer.
    //
    // Gated on the chroma being concentrated at all: with no tonal centre
    // there is no reference pitch to find, and reporting one would be
    // inventing a number.
    if (concentration > 0.10)
    {
        double sumSin = 0.0;
        double sumCos = 0.0;
        double weight = 0.0;

        for (int i = 0; i < kFineBins; ++i)
        {
            const double value = fineHistogram[static_cast<std::size_t> (i)];
            if (value <= 0.0)
                continue;

            // Position within the octave, then the distance to the nearest
            // semitone, in semitones.
            const double position  = static_cast<double> (i) / 100.0;
            const double deviation = position - std::round (position);

            const double angle = 2.0 * kPi * deviation;
            sumSin += value * std::sin (angle);
            sumCos += value * std::cos (angle);
            weight += value;
        }

        if (weight > 1e-12)
        {
            // Circular mean, because the deviation wraps at half a semitone.
            const double meanDeviation = std::atan2 (sumSin, sumCos) / (2.0 * kPi);

            result.tuningValid = true;
            result.tuningCents = meanDeviation * 100.0;
            result.referenceHz = 440.0 * std::pow (2.0, meanDeviation / 12.0);
        }
    }


    if (candidates.size() > 6)
        candidates.resize (6);

    result.candidates = std::move (candidates);
    return result;
}

} // namespace tmix

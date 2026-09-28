#include "TempoEstimator.h"

#include "Fft.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace tmix {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Harmonic weights for the comb score. The fundamental carries the most
// weight, which is what stops the score drifting to a subdivision level where
// the material has no component at the fundamental.
constexpr double kHarmonicWeights[] = { 1.0, 0.5, 1.0 / 3.0, 0.25 };
constexpr int    kHarmonicCount = 4;

constexpr int    kRefineSteps        = 240;
constexpr double kRefineSpan         = 0.06; // +/- 6%, wider than one ACF lag
constexpr std::size_t kMaxCoarsePeaks = 6;

constexpr double kDuplicateFraction = 0.005;

// A grid counts as half empty - and so as twice the real event rate - below
// this filling. Measuring fill on real material put the readings that are
// genuinely an octave too fast at 0.30 and 0.54, while readings that are
// merely accented every other beat sat at 0.79 and above.
constexpr double kHalfEmptyFilling = 0.65;

// How close the two readings' comb evidence has to be before filling is asked
// to separate them. At or above this the slower reading is a real contender;
// well below it the search has already made up its mind.
constexpr double kTossUpCombRatio = 0.8;

// Deliberately gentle. The prior exists to nudge genuinely ambiguous material
// toward ordinary tempos; at full strength it overrode clearly correct
// readings (it turned a 60 BPM click track into 120), so it is raised to a
// fractional power instead of applied directly.
constexpr double kPriorExponent = 0.25;

struct Candidate
{
    double bpm                = 0.0;
    double periodSeconds      = 0.0;
    double combScore          = 0.0;
    double peakCombScore      = 0.0;
    double lowBandCombScore   = 0.0;
    double filling            = 0.0;
    double gridMean           = 0.0;
    double phaseSeconds       = 0.0;
    double alignmentProminence = 0.0; // how much better the best alignment is
    double score              = 0.0;
};

// FFT-based autocorrelation (Wiener-Khinchin), normalised so lag 0 is 1.
std::vector<double> autocorrelation (const std::vector<double>& signal)
{
    const std::size_t n = signal.size();

    std::size_t size = 1;
    while (size < 2 * n)
        size <<= 1;

    Fft fft (size);
    std::vector<std::complex<float>> data (size, { 0.0f, 0.0f });
    for (std::size_t i = 0; i < n; ++i)
        data[i] = { static_cast<float> (signal[i]), 0.0f };

    fft.forward (data.data());

    for (auto& value : data)
    {
        const float power = value.real() * value.real() + value.imag() * value.imag();
        value = { power, 0.0f };
    }

    fft.inverse (data.data());

    std::vector<double> result (n, 0.0);
    const double zeroLag = static_cast<double> (data[0].real());

    if (zeroLag > 0.0)
        for (std::size_t lag = 0; lag < n; ++lag)
            result[lag] = static_cast<double> (data[lag].real()) / zeroLag;

    return result;
}

// Sum of |Z_k(P)| over the first few harmonics: how much pulse-train energy
// sits at this period. Evaluated by complex rotation rather than per-sample
// sin/cos, which keeps the dense search affordable.
double combScore (const std::vector<double>& odf,
                  const std::vector<double>& window,
                  double hopSeconds,
                  double periodSeconds)
{
    double total = 0.0;

    for (int k = 1; k <= kHarmonicCount; ++k)
    {
        const double delta = -2.0 * kPi * static_cast<double> (k) * hopSeconds
                             / periodSeconds;

        const std::complex<double> step (std::cos (delta), std::sin (delta));
        std::complex<double> rotation (1.0, 0.0);
        std::complex<double> sum (0.0, 0.0);

        for (std::size_t t = 0; t < odf.size(); ++t)
        {
            sum += (odf[t] * window[t]) * rotation;
            rotation *= step;
        }

        total += kHarmonicWeights[k - 1] * std::abs (sum);
    }

    return total;
}

// Mean onset strength sampled at the grid positions of a candidate period.
//
// This is the measure that resolves the central ambiguity of tempo detection:
// an evenly spaced click track is *also* consistent with double its rate,
// because an impulse train contains every harmonic. What separates the two is
// that a grid at double the rate leaves half its positions empty, which halves
// the mean. A grid that is fully occupied is the real event rate.
double gridMean (const std::vector<float>& envelope,
                 double hopSeconds,
                 double frameOffset,
                 double periodSeconds,
                 double phaseSeconds)
{
    if (periodSeconds <= 0.0 || envelope.empty())
        return 0.0;

    const double lastFrameTime = frameOffset
        + static_cast<double> (envelope.size() - 1) * hopSeconds;

    const auto frameCount = static_cast<long long> (envelope.size());

    double sum = 0.0;
    std::size_t positions = 0;

    for (int step = 0; ; ++step)
    {
        const double time = phaseSeconds + static_cast<double> (step) * periodSeconds;
        if (time > lastFrameTime)
            break;

        const auto nearest = static_cast<long long> (
            std::llround ((time - frameOffset) / hopSeconds));

        // Take the best of a small neighbourhood: the flux peak trails the
        // true onset by a few milliseconds, and a grid position falling
        // between frames must not be penalised for that.
        double best = 0.0;
        for (long long offset = -2; offset <= 2; ++offset)
        {
            const long long index = nearest + offset;
            if (index >= 0 && index < frameCount)
                best = std::max (best, static_cast<double> (envelope[static_cast<std::size_t> (index)]));
        }

        sum += best;
        ++positions;
    }

    return positions > 0 ? sum / static_cast<double> (positions) : 0.0;
}

// Occupancy balance between the two halves of a grid.
//
// The even positions of a grid at period P are exactly the grid at 2P, so the
// odd positions are precisely the ones that would be empty if the true rate
// were half of 1/P. A ratio near 1 means the grid is uniformly occupied and
// 1/P is the real event rate; a ratio near 0 means every other position is
// empty and the real rate is 1/(2P).
//
// This is the scale-invariant counterpart to gridMean. A plain mean onset
// strength favours slower periods simply because they have fewer, better
// spaced positions - which biases every answer toward half tempo.
// Measurements taken across a grid's positions.
//
// The even/odd means are what the filling ratio is built from. The occupancy
// count is the part a plain ratio cannot see: it reports how many positions
// carry anything at all, which is the difference between a grid that is
// genuinely full and one that is uniformly empty.
struct GridStats
{
    double evenMean  = 0.0;
    double oddMean   = 0.0;
    double mean      = 0.0; // across both halves
    double occupancy = 0.0; // share of positions above the threshold
    std::size_t positions = 0;
};

template <typename Sample>
GridStats measureGrid (const std::vector<Sample>& envelope,
                       double hopSeconds,
                       double frameOffset,
                       double periodSeconds,
                       double phaseSeconds,
                       double occupancyThreshold)
{
    GridStats stats;

    if (periodSeconds <= 0.0 || envelope.empty())
        return stats;

    const double lastFrameTime = frameOffset
        + static_cast<double> (envelope.size() - 1) * hopSeconds;

    const auto frameCount = static_cast<long long> (envelope.size());

    double evenSum = 0.0;
    double oddSum  = 0.0;
    std::size_t evenCount = 0;
    std::size_t oddCount  = 0;
    std::size_t occupied  = 0;

    for (int step = 0; ; ++step)
    {
        const double time = phaseSeconds + static_cast<double> (step) * periodSeconds;
        if (time > lastFrameTime)
            break;

        const auto nearest = static_cast<long long> (
            std::llround ((time - frameOffset) / hopSeconds));

        double best = 0.0;
        for (long long offset = -2; offset <= 2; ++offset)
        {
            const long long index = nearest + offset;
            if (index >= 0 && index < frameCount)
                best = std::max (best, static_cast<double> (envelope[static_cast<std::size_t> (index)]));
        }

        if (best > occupancyThreshold)
            ++occupied;

        if (step % 2 == 0)
        {
            evenSum += best;
            ++evenCount;
        }
        else
        {
            oddSum += best;
            ++oddCount;
        }
    }

    stats.positions = evenCount + oddCount;

    if (stats.positions > 0)
    {
        stats.mean = (evenSum + oddSum) / static_cast<double> (stats.positions);
        stats.occupancy = static_cast<double> (occupied)
                          / static_cast<double> (stats.positions);
    }

    if (evenCount > 0)
        stats.evenMean = evenSum / static_cast<double> (evenCount);

    if (oddCount > 0)
        stats.oddMean = oddSum / static_cast<double> (oddCount);

    return stats;
}

double gridFilling (const std::vector<float>& envelope,
                    double hopSeconds,
                    double frameOffset,
                    double periodSeconds,
                    double phaseSeconds)
{
    const GridStats stats = measureGrid (envelope, hopSeconds, frameOffset,
                                         periodSeconds, phaseSeconds, 0.0);

    if (stats.positions == 0)
        return 0.0;

    const double high = std::max (stats.evenMean, stats.oddMean);

    return high > 0.0 ? std::min (stats.evenMean, stats.oddMean) / high : 0.0;
}

struct GridFit
{
    double phaseSeconds = 0.0;
    double bestMean     = 0.0; // mean onset strength at the chosen alignment
    double averageMean  = 0.0; // mean across all alignments
};

// Finds the grid alignment that captures the most onset strength.
//
// This deliberately does NOT use the argument of the first harmonic. That is
// an onset-weighted circular mean, and it collapses when events sit at both a
// grid position and halfway between - which is exactly what a hat playing
// eighths under a quarter-note beat produces. Sampling every alignment
// directly stays correct for that case and for subdivided material generally.
GridFit fitGrid (const std::vector<float>& envelope,
                 double hopSeconds,
                 double frameOffset,
                 double periodSeconds)
{
    constexpr int kPhaseSteps = 64;

    GridFit fit;

    if (periodSeconds <= 0.0 || envelope.empty())
        return fit;

    std::vector<double> means (kPhaseSteps, 0.0);

    int bestIndex = 0;

    for (int i = 0; i < kPhaseSteps; ++i)
    {
        const double phase = periodSeconds * static_cast<double> (i)
                             / static_cast<double> (kPhaseSteps);

        means[static_cast<std::size_t> (i)] =
            gridMean (envelope, hopSeconds, frameOffset, periodSeconds, phase);

        fit.averageMean += means[static_cast<std::size_t> (i)];

        if (means[static_cast<std::size_t> (i)] > means[static_cast<std::size_t> (bestIndex)])
            bestIndex = i;
    }

    fit.averageMean /= static_cast<double> (kPhaseSteps);
    fit.bestMean = means[static_cast<std::size_t> (bestIndex)];

    double phase = periodSeconds * static_cast<double> (bestIndex)
                   / static_cast<double> (kPhaseSteps);

    // Parabolic refinement, wrapping at the ends because phase is circular.
    const double y0 = means[static_cast<std::size_t> ((bestIndex + kPhaseSteps - 1) % kPhaseSteps)];
    const double y1 = means[static_cast<std::size_t> (bestIndex)];
    const double y2 = means[static_cast<std::size_t> ((bestIndex + 1) % kPhaseSteps)];

    const double denominator = y0 - 2.0 * y1 + y2;

    if (std::fabs (denominator) > 1e-12)
    {
        const double shift = 0.5 * (y0 - y2) / denominator;

        if (std::fabs (shift) < 1.0)
            phase += shift * periodSeconds / static_cast<double> (kPhaseSteps);
    }

    phase = std::fmod (phase, periodSeconds);
    if (phase < 0.0)
        phase += periodSeconds;

    fit.phaseSeconds = phase;
    return fit;
}

double priorWeight (double bpm, const TempoConfig& config)
{
    if (bpm <= 0.0 || config.priorSigmaOctaves <= 0.0)
        return 1.0;

    const double octaves =
        std::log2 (bpm / config.priorCentreBpm) / config.priorSigmaOctaves;
    return std::exp (-0.5 * octaves * octaves);
}

Candidate refine (const std::vector<double>& odf,
                  const std::vector<double>& window,
                  double hopSeconds,
                  double coarsePeriod)
{
    Candidate best;

    const double lo = coarsePeriod * (1.0 - kRefineSpan);
    const double hi = coarsePeriod * (1.0 + kRefineSpan);
    const double ratio = hi / lo;

    std::vector<double> periods (kRefineSteps, 0.0);
    std::vector<double> scores (kRefineSteps, 0.0);

    int bestIndex = 0;

    for (int i = 0; i < kRefineSteps; ++i)
    {
        const double fraction = static_cast<double> (i)
                                / static_cast<double> (kRefineSteps - 1);
        const double period = lo * std::pow (ratio, fraction);

        periods[i] = period;
        scores[i]  = combScore (odf, window, hopSeconds, period);

        if (scores[i] > scores[bestIndex])
            bestIndex = i;
    }

    double refinedPeriod = periods[static_cast<std::size_t> (bestIndex)];

    // Parabolic interpolation across the peak's neighbours, so the answer is
    // not limited to the search grid.
    if (bestIndex > 0 && bestIndex + 1 < kRefineSteps)
    {
        const double y0 = scores[static_cast<std::size_t> (bestIndex - 1)];
        const double y1 = scores[static_cast<std::size_t> (bestIndex)];
        const double y2 = scores[static_cast<std::size_t> (bestIndex + 1)];

        const double denominator = y0 - 2.0 * y1 + y2;

        if (std::fabs (denominator) > 1e-12)
        {
            const double shift = 0.5 * (y0 - y2) / denominator;

            if (std::fabs (shift) < 1.0)
            {
                const double step = periods[static_cast<std::size_t> (bestIndex + 1)]
                                    - periods[static_cast<std::size_t> (bestIndex)];
                refinedPeriod += shift * step;
            }
        }
    }

    best.periodSeconds = refinedPeriod;
    best.bpm           = 60.0 / refinedPeriod;
    best.combScore     = scores[static_cast<std::size_t> (bestIndex)];

    return best;
}

} // namespace

TempoResult estimateTempo (const OnsetEnvelope& envelope, const TempoConfig& config)
{
    TempoResult result;

    const std::size_t frameCount = envelope.frameCount();

    if (frameCount < 32 || envelope.hopSeconds <= 0.0
        || config.minBpm <= 0.0 || config.maxBpm <= config.minBpm)
        return result;

    const double hopSeconds = envelope.hopSeconds;

    std::vector<double> odf (frameCount, 0.0);
    double mean = 0.0;
    for (std::size_t t = 0; t < frameCount; ++t)
        mean += static_cast<double> (envelope.full[t]);
    mean /= static_cast<double> (frameCount);

    double rawEnergy = 0.0;
    for (std::size_t t = 0; t < frameCount; ++t)
    {
        odf[t] = static_cast<double> (envelope.full[t]) - mean;
        rawEnergy += static_cast<double> (envelope.full[t])
                     * static_cast<double> (envelope.full[t]);
    }

    if (rawEnergy <= 1e-9)
        return result; // Silence reports no tempo.

    std::vector<double> window (frameCount, 1.0);
    if (frameCount > 1)
        for (std::size_t t = 0; t < frameCount; ++t)
            window[t] = 0.5 - 0.5 * std::cos (2.0 * kPi * static_cast<double> (t)
                                              / static_cast<double> (frameCount - 1));

    // Peak-emphasised copy of the envelope.
    //
    // A hat pattern playing eighths can be louder than the kick and snare that
    // define the beat, in which case the comb score happily locks onto the hat
    // rate and reports double time. Subtracting a high percentile removes the
    // quiet subdivision events and leaves the salient ones, which is closer to
    // what a listener counts.
    std::vector<double> peakOdf (frameCount, 0.0);
    {
        std::vector<float> sorted (envelope.full);
        std::nth_element (sorted.begin(),
                          sorted.begin() + static_cast<std::ptrdiff_t> (frameCount * 6 / 10),
                          sorted.end());

        const double threshold =
            static_cast<double> (sorted[frameCount * 6 / 10]);

        for (std::size_t t = 0; t < frameCount; ++t)
            peakOdf[t] = std::max (0.0, static_cast<double> (envelope.full[t]) - threshold);
    }

    // Mean-removed low band. Kicks live here and hats do not, so this band is
    // the one that identifies the beat rather than a subdivision of it.
    std::vector<double> lowOdf;
    double lowBandEnergy = 0.0;

    if (envelope.low.size() == frameCount)
    {
        lowOdf.assign (frameCount, 0.0);

        double lowMean = 0.0;
        for (std::size_t t = 0; t < frameCount; ++t)
            lowMean += static_cast<double> (envelope.low[t]);
        lowMean /= static_cast<double> (frameCount);

        for (std::size_t t = 0; t < frameCount; ++t)
        {
            lowOdf[t] = static_cast<double> (envelope.low[t]) - lowMean;
            lowBandEnergy += lowOdf[t] * lowOdf[t];
        }
    }

    // ---- coarse candidates from the autocorrelation ----
    const auto acf = autocorrelation (odf);

    const double minLag = (60.0 / config.maxBpm) / hopSeconds;
    const double maxLag = (60.0 / config.minBpm) / hopSeconds;

    struct Peak { std::size_t lag; double value; };
    std::vector<Peak> peaks;

    const auto firstLag = static_cast<std::size_t> (std::max (2.0, std::ceil (minLag)));
    const auto lastLag  = std::min (frameCount - 2,
                                    static_cast<std::size_t> (std::floor (maxLag)));

    for (std::size_t lag = firstLag; lag <= lastLag; ++lag)
        if (acf[lag] >= acf[lag - 1] && acf[lag] > acf[lag + 1] && acf[lag] > 0.0)
            peaks.push_back ({ lag, acf[lag] });

    std::sort (peaks.begin(), peaks.end(),
               [] (const Peak& a, const Peak& b) { return a.value > b.value; });

    if (peaks.size() > kMaxCoarsePeaks)
        peaks.resize (kMaxCoarsePeaks);

    // Sweep across the range if the autocorrelation found nothing usable, so a
    // weak but real pulse train is still found.
    if (peaks.empty())
    {
        const double midLag = std::sqrt (minLag * maxLag);
        for (double factor : { 0.4, 0.7, 1.0, 1.4 })
            peaks.push_back ({ 0, midLag * factor });
    }

    // ---- refine, and carry each coarse period's half and double too ----
    std::vector<Candidate> candidates;

    for (const auto& peak : peaks)
    {
        const double coarsePeriod = peak.lag > 0
            ? static_cast<double> (peak.lag) * hopSeconds
            : 60.0 / config.priorCentreBpm;

        for (const double scale : { 1.0, 2.0, 0.5 })
        {
            const double period = coarsePeriod * scale;
            const double bpm = 60.0 / period;

            if (bpm < config.minBpm * 0.9 || bpm > config.maxBpm * 1.1)
                continue;

            Candidate candidate = refine (odf, window, hopSeconds, period);

            // The range is a hint about where to look, not a hard wall. A
            // strict comparison here silently discarded material sitting
            // exactly on the boundary: a 60 BPM click track refined to
            // 59.99999 and was thrown away, leaving double tempo to win.
            if (candidate.bpm < config.minBpm * 0.98
                || candidate.bpm > config.maxBpm * 1.02)
                continue;

            candidates.push_back (candidate);
        }
    }

    if (candidates.empty())
        return result;

    std::sort (candidates.begin(), candidates.end(),
               [] (const Candidate& a, const Candidate& b)
               { return a.combScore > b.combScore; });

    std::vector<Candidate> unique;
    for (const auto& candidate : candidates)
    {
        bool duplicate = false;
        for (const auto& kept : unique)
            if (std::fabs (candidate.bpm - kept.bpm) / kept.bpm < kDuplicateFraction)
                duplicate = true;

        if (! duplicate)
            unique.push_back (candidate);
    }

    // ---- measure each candidate, then combine ----
    for (auto& candidate : unique)
    {
        const GridFit fit = fitGrid (envelope.full, hopSeconds, envelope.frameOffset,
                                     candidate.periodSeconds);

        candidate.phaseSeconds = fit.phaseSeconds;
        candidate.gridMean     = fit.bestMean;

        candidate.alignmentProminence = fit.bestMean > 0.0
            ? std::max (0.0, fit.bestMean - fit.averageMean) / fit.bestMean
            : 0.0;

        candidate.peakCombScore =
            combScore (peakOdf, window, hopSeconds, candidate.periodSeconds);

        candidate.filling = gridFilling (envelope.full, hopSeconds, envelope.frameOffset,
                                         candidate.periodSeconds, fit.phaseSeconds);

        if (! lowOdf.empty())
            candidate.lowBandCombScore =
                combScore (lowOdf, window, hopSeconds, candidate.periodSeconds);
    }

    double maxComb = 0.0;
    double maxGrid = 0.0;
    for (const auto& candidate : unique)
    {
        maxComb = std::max (maxComb, candidate.combScore);
        maxGrid = std::max (maxGrid, candidate.gridMean);
    }

    if (maxComb <= 0.0 || maxGrid <= 0.0)
        return result;

    for (auto& candidate : unique)
    {
        const double combNorm = candidate.combScore / maxComb;

        // Filling replaces the raw grid mean in the score. The grid mean is not
        // scale invariant - it prefers slower periods because they have fewer
        // positions to fill - and on real material that bias was cancelling
        // the comb score's opposite bias rather than measuring anything.
        const double prior = std::pow (priorWeight (candidate.bpm, config), kPriorExponent);

        candidate.score = combNorm * prior;
    }

    // Tuning aid: set TMIX_DEBUG_TEMPO=1 to see how each candidate was scored.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4996) // getenv is flagged as unsafe; it is only read.
#endif
    const bool debugTempo = std::getenv ("TMIX_DEBUG_TEMPO") != nullptr;
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

    if (debugTempo)
    {
        for (const auto& candidate : unique)
        {
            const GridStats stats = measureGrid (envelope.full, hopSeconds,
                                                 envelope.frameOffset,
                                                 candidate.periodSeconds,
                                                 candidate.phaseSeconds, mean);

            GridStats lowStats;
            if (! lowOdf.empty())
                lowStats = measureGrid (lowOdf, hopSeconds, envelope.frameOffset,
                                        candidate.periodSeconds,
                                        candidate.phaseSeconds, 0.0);

            std::printf ("      %8.3f bpm  P %7.4f  phase %7.4f  comb %10.2f  peakcomb %10.2f  fill %5.3f  grid %8.4f  conc %5.3f  score %6.4f  | even %8.2f  odd %8.2f  occ %5.3f  pos %5zu\n",
                         candidate.bpm, candidate.periodSeconds, candidate.phaseSeconds,
                         candidate.combScore, candidate.peakCombScore, candidate.filling, candidate.gridMean,
                         candidate.alignmentProminence, candidate.score,
                         stats.evenMean, stats.oddMean, stats.occupancy, stats.positions);

            std::printf ("                lowcomb %10.2f  lowgrid %8.4f  loweven %8.4f  lowodd %8.4f  lowpos %5zu\n",
                         candidate.lowBandCombScore, lowStats.mean,
                         lowStats.evenMean, lowStats.oddMean, lowStats.positions);
        }
    }

    std::sort (unique.begin(), unique.end(),
               [] (const Candidate& a, const Candidate& b) { return a.score > b.score; });

    // The octave question, asked once, where it is actually well posed.
    //
    // Filling says whether the alternate positions of a grid are empty. That
    // is exactly what separates a reading from its own octave - and it is the
    // only question it answers. Multiplied into every candidate's score it
    // also handed a high mark to the 3:2 relative (150 for material at 100),
    // where two equally filled halves follow from the ratio of the periods
    // rather than from anything in the music.
    //
    // So it is asked here instead, between the strongest reading and its half.
    // Only when the slower of the two is the better filled is the faster one
    // an octave rather than the rate a listener counts.
    std::size_t bestIndex = 0;
    {
        const Candidate& fastest = unique.front();
        const double halfBpm = fastest.bpm / 2.0;

        for (std::size_t i = 1; i < unique.size(); ++i)
        {
            if (std::fabs (unique[i].bpm - halfBpm) / halfBpm > kDuplicateFraction)
                continue;

            // Filling decides a toss-up, and only a toss-up.
            //
            // When both readings are equally periodic - an eighth-note pulse is
            // as regular at 150 as at 75 - the search has nothing to choose
            // between them and filling is the only evidence left. When it
            // already prefers one by a clear margin, there is no tie to break:
            // asking anyway overturned correct answers on material that
            // accents every other beat, where the faster grid is genuinely
            // half empty but is still the rate a listener counts.
            const double slowerEvidence = fastest.combScore > 0.0
                ? unique[i].combScore / fastest.combScore
                : 0.0;

            if (fastest.filling < kHalfEmptyFilling
                && unique[i].filling > fastest.filling
                && slowerEvidence > kTossUpCombRatio)
                bestIndex = i;

            break;
        }
    }

    const Candidate& best = unique[bestIndex];

    result.valid              = true;
    result.rawBpm             = best.bpm;
    result.bpm                = best.bpm;
    result.periodSeconds      = best.periodSeconds;
    result.firstBeatSeconds   = best.phaseSeconds;
    result.phaseConcentration = best.alignmentProminence;

    // ---- confidence ----
    const double runnerUp = unique.size() > 1
        ? unique[bestIndex == 0 ? 1 : 0].score
        : 0.0;
    const double margin = best.score > 0.0
        ? std::max (0.0, best.score - runnerUp) / best.score
        : 0.0;

    result.confidence = 0.5 * std::min (1.0, margin / 0.30)
                        + 0.5 * std::min (1.0, best.alignmentProminence / 0.6);
    result.uncertain = result.confidence < config.confidenceThreshold;

    // ---- candidates offered to the user ----
    result.candidates.clear();
    result.candidates.push_back ({ best.bpm, 1.0, false, false });

    const double halfBpm   = best.bpm / 2.0;
    const double doubleBpm = best.bpm * 2.0;

    for (const auto& candidate : unique)
    {
        if (result.candidates.size() >= 5)
            break;

        const double ratio = candidate.bpm / best.bpm;

        bool listed = false;
        for (const auto& existing : result.candidates)
            if (std::fabs (existing.bpm - candidate.bpm) / candidate.bpm < kDuplicateFraction)
                listed = true;

        if (listed)
            continue;

        const bool isHalf   = std::fabs (ratio - 0.5) < 0.02
                              || std::fabs (candidate.bpm - halfBpm) / halfBpm < kDuplicateFraction;
        const bool isDouble = std::fabs (ratio - 2.0) < 0.02
                              || std::fabs (candidate.bpm - doubleBpm) / doubleBpm < kDuplicateFraction;

        if (candidate.bpm < config.minBpm || candidate.bpm > config.maxBpm)
            continue;

        result.candidates.push_back ({ candidate.bpm,
                                       best.score > 0.0 ? candidate.score / best.score : 0.0,
                                       isHalf, isDouble });

        if (isHalf)
            result.lowBandAlignment = candidate.gridMean;
    }

    // The half and double relatives are always offered, even when the search
    // did not happen to evaluate them, because they are the readings the user
    // most often needs to switch between.
    for (const auto pair : { std::make_pair (halfBpm, true), std::make_pair (doubleBpm, false) })
    {
        const double bpm = pair.first;
        if (bpm < config.minBpm || bpm > config.maxBpm)
            continue;

        bool listed = false;
        for (const auto& existing : result.candidates)
            if (std::fabs (existing.bpm - bpm) / bpm < kDuplicateFraction)
                listed = true;

        if (listed)
            continue;

        const double period = 60.0 / bpm;

        const GridFit fit = fitGrid (envelope.full, hopSeconds,
                                     envelope.frameOffset, period);

        const double combNorm = maxComb > 0.0
            ? combScore (odf, window, hopSeconds, period) / maxComb : 0.0;

        const double filling = gridFilling (envelope.full, hopSeconds,
                                            envelope.frameOffset, period, fit.phaseSeconds);

        result.candidates.push_back ({ bpm, combNorm * filling,
                                       pair.second, ! pair.second });
    }

    std::sort (result.candidates.begin() + 1, result.candidates.end(),
               [] (const TempoCandidate& a, const TempoCandidate& b)
               { return a.score > b.score; });

    // ---- octave folding into the declared preferred range -------------------
    // Each step moves strictly closer to the range and the loop condition is
    // bounded by the range itself, so this cannot run away.
    if (config.preferredMaxBpm > config.preferredMinBpm)
    {
        while (result.bpm > config.preferredMaxBpm
               && result.bpm / 2.0 >= config.preferredMinBpm)
            result.bpm /= 2.0;

        while (result.bpm < config.preferredMinBpm
               && result.bpm * 2.0 <= config.preferredMaxBpm)
            result.bpm *= 2.0;

        result.folded = std::fabs (result.bpm - result.rawBpm) > 1e-9;

        // Re-express the candidate list around the octave actually reported.
        // Leaving the search's own pick at "100% primary" under a headline of
        // a different number reads as a contradiction.
        if (result.folded)
        {
            double reportedScore = 0.0;
            for (const auto& candidate : result.candidates)
                if (std::fabs (candidate.bpm - result.bpm) / result.bpm < kDuplicateFraction)
                    reportedScore = candidate.score;

            std::vector<TempoCandidate> rebuilt;
            // A negative score marks the reported value. Giving it 1.0 would
            // print two candidates at 100%, since the search's own pick often
            // scores higher than the octave it was folded to.
            rebuilt.push_back ({ result.bpm, -1.0, false, false });

            const auto flag = [&result] (double bpm) {
                const double ratio = bpm / result.bpm;
                return std::make_pair (std::fabs (ratio - 0.5) < 0.02,
                                       std::fabs (ratio - 2.0) < 0.02);
            };

            for (const auto& candidate : result.candidates)
            {
                if (std::fabs (candidate.bpm - result.bpm) / result.bpm < kDuplicateFraction)
                    continue;

                const auto flags = flag (candidate.bpm);
                rebuilt.push_back ({ candidate.bpm, candidate.score,
                                     flags.first, flags.second });
            }

            bool rawPresent = false;
            for (const auto& candidate : rebuilt)
                if (std::fabs (candidate.bpm - result.rawBpm) / result.rawBpm
                    < kDuplicateFraction)
                    rawPresent = true;

            if (! rawPresent && result.rawBpm > 0.0
                && rebuilt.size() < 5)
            {
                const auto flags = flag (result.rawBpm);
                rebuilt.push_back ({ result.rawBpm,
                                     reportedScore > 0.0 ? reportedScore : 0.5,
                                     flags.first, flags.second });
            }

            std::sort (rebuilt.begin() + 1, rebuilt.end(),
                       [] (const TempoCandidate& a, const TempoCandidate& b)
                       { return a.score > b.score; });

            result.candidates = std::move (rebuilt);
        }
    }

    return result;
}

} // namespace tmix

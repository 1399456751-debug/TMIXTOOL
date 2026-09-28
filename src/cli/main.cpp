#include "Analysis.h"
#include "AudioFile.h"
#include "Decimator.h"
#include "MixTiming.h"
#include "NoteTiming.h"
#include "OnsetDetector.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#  define NOMINMAX // windows.h defines min/max as macros, which breaks std::min
#  include <windows.h>
#endif

namespace {

struct CommandLine
{
    std::filesystem::path path;
    bool quiet = false;
    bool dumpOnsets = false;
    bool ok    = false;
};

void printUsage()
{
    std::printf ("TMIXCLI - command line front end for the TMIXTOOL analysis core\n\n");
    std::printf ("usage: TMIXCLI <audio-file> [--quiet] [--dump-onsets]\n\n");
    std::printf ("  --quiet        report only the detected tempo\n");
    std::printf ("  --dump-onsets  print the onset peak times and their interval histogram\n");
}

void printLevel (const char* label, double linear, double db)
{
    std::printf ("%-12s: %.6f  (%.2f dBFS)\n", label, linear, db);
}

void printNoteValues (const tmix::NoteTiming& timing)
{
    std::printf ("\nnote values at %.2f BPM\n", timing.bpm());
    std::printf ("%-8s %12s %12s\n", "note", "ms", "Hz");

    for (const auto& entry : timing.table())
        std::printf ("%-8s %12.2f %12.3f\n",
                     entry.label.c_str(), entry.ms, entry.hz);
}

void printTimeTable (const char* title,
                     const std::vector<tmix::TimeValue>& values)
{
    std::printf ("\n%s\n", title);
    std::printf ("%-8s %10s %20s\n", "", "ms", "when");

    for (const auto& value : values)
        std::printf ("%-8s %10.2f  %s\n",
                     value.label.c_str(), value.ms, value.use.c_str());
}

void printMixTiming (const tmix::MixTiming& timing)
{
    std::printf ("\ndelay at %.2f BPM\n", timing.bpm());
    for (const auto& preset : timing.delayPresets())
        std::printf ("  %-16s left %8.2f ms   right %8.2f ms\n",
                     preset.name.c_str(), preset.leftMs, preset.rightMs);

    printTimeTable ("compressor attack", timing.attackSuggestions());
    printTimeTable ("compressor release", timing.releaseSuggestions());
    printTimeTable ("release, beat-synced (effect)", timing.releaseSyncedOptions());

    std::printf ("\nreverb decay\n");
    for (const auto& row : timing.reverbDecays())
        std::printf ("  %-8s %8.2f ms  %s\n",
                     row.label.c_str(), row.ms, row.use.c_str());

    printTimeTable ("reverb pre-delay (Haas threshold 30 ms)",
                    timing.preDelays());
}

// Debug view of the onset stage: where the events actually are, and how far
// apart they sit. Tempo scoring arguments are much easier to settle against
// the event times than against the score alone.
int dumpOnsets (const std::filesystem::path& path)
{
    tmix::DecodedAudio audio;
    std::string error;

    if (! tmix::decodeAudioFile (path, audio, error))
    {
        std::printf ("error: %s\n", error.c_str());
        return 1;
    }

    const auto frames = static_cast<std::size_t> (audio.frameCount());
    std::vector<float> mono (frames, 0.0f);

    if (audio.channels <= 1)
    {
        mono = audio.samples;
        mono.resize (frames);
    }
    else
    {
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            double sum = 0.0;
            for (int channel = 0; channel < audio.channels; ++channel)
                sum += static_cast<double> (audio.samples[frame * audio.channels + channel]);

            mono[frame] = static_cast<float> (sum / audio.channels);
        }
    }

    tmix::Decimator decimator (audio.sampleRate, 22050);
    const auto analysis = decimator.process (mono);
    const auto envelope = tmix::computeOnsetEnvelope (analysis, decimator.outputRate());

    const auto& odf = envelope.full;

    double mean = 0.0;
    double maxValue = 0.0;
    for (const float value : odf)
    {
        mean += value;
        maxValue = std::max (maxValue, static_cast<double> (value));
    }
    mean /= static_cast<double> (odf.size());

    const double threshold = mean + 0.15 * (maxValue - mean);

    std::vector<double> peaks;
    for (std::size_t i = 3; i + 3 < odf.size(); ++i)
    {
        if (odf[i] < threshold)
            continue;

        bool isPeak = true;
        for (int offset = -3; offset <= 3; ++offset)
            if (offset != 0 && odf[i + offset] > odf[i])
                isPeak = false;

        if (! isPeak)
            continue;

        const double time = envelope.frameTime (i);
        if (peaks.empty() || time - peaks.back() > 0.05)
            peaks.push_back (time);
    }

    std::printf ("frames %zu  hop %.4f s  mean %.2f  max %.2f  threshold %.2f\n",
                 odf.size(), envelope.hopSeconds, mean, maxValue, threshold);
    std::printf ("peaks %zu\n\n", peaks.size());

    constexpr int kBuckets = 120; // 0-1200 ms in 10 ms steps
    std::vector<int> histogram (kBuckets, 0);

    for (std::size_t i = 1; i < peaks.size(); ++i)
    {
        const double delta = peaks[i] - peaks[i - 1];
        const int bucket = static_cast<int> (delta * 100.0);
        if (bucket >= 0 && bucket < kBuckets)
            ++histogram[static_cast<std::size_t> (bucket)];
    }

    std::printf ("interval histogram (10 ms buckets)\n");
    for (int bucket = 0; bucket < kBuckets; ++bucket)
    {
        const int count = histogram[static_cast<std::size_t> (bucket)];
        if (count <= 0)
            continue;

        const int barLength = std::min (count, 60);
        std::printf ("  %4d-%4d ms  %5d  %s\n", bucket * 10, bucket * 10 + 10, count,
                     std::string (static_cast<std::size_t> (barLength), '#').c_str());
    }

    std::printf ("\nfirst 60 peaks (seconds)\n");
    for (std::size_t i = 0; i < peaks.size() && i < 60; ++i)
        std::printf ("  %8.4f%s", peaks[i], (i % 6 == 5) ? "\n" : "");
    std::printf ("\n");

    return 0;
}

int run (const CommandLine& command)
{
    if (! command.ok)
    {
        printUsage();
        return 2;
    }

    if (command.dumpOnsets)
        return dumpOnsets (command.path);

    const auto result = tmix::analyseFile (command.path);

    if (! result.ok)
    {
        std::printf ("error: %s\n", result.error.c_str());
        return 1;
    }

    if (command.quiet)
    {
        if (result.tempo.valid)
            std::printf ("%.4f\n", result.tempo.bpm);
        else
            std::printf ("no tempo detected\n");

        return result.tempo.valid ? 0 : 1;
    }

    const double seconds = result.durationSeconds;

    std::printf ("file        : %s\n", command.path.u8string().c_str());
    std::printf ("sample rate : %d Hz\n", result.sampleRate);
    std::printf ("channels    : %d\n", result.channels);
    std::printf ("frames      : %lld\n", result.frameCount);
    std::printf ("duration    : %.3f s (%.2f min)\n", seconds, seconds / 60.0);
    printLevel ("peak", result.levels.peak, result.levels.peakDb);
    printLevel ("rms", result.levels.rms, result.levels.rmsDb);

    const auto& tempo = result.tempo;

    if (! tempo.valid)
    {
        std::printf ("\ntempo       : no tempo detected\n");
        std::printf ("              The material may be silent, or too short or too\n");
        std::printf ("              arrhythmic for a stable pulse.\n");
        return 1;
    }

    std::printf ("\ntempo       : %.2f BPM", tempo.bpm);
    if (tempo.uncertain)
        std::printf ("   [UNCERTAIN]");
    std::printf ("\n");

    if (tempo.folded)
        std::printf ("              (search found %.2f, folded to fit the preferred range)\n",
                     tempo.rawBpm);

    std::printf ("confidence  : %.2f\n", tempo.confidence);
    std::printf ("grid starts : %.4f s\n", tempo.firstBeatSeconds);

    std::printf ("\ncandidates\n");
    for (const auto& candidate : tempo.candidates)
    {
        std::printf ("  %8.2f BPM", candidate.bpm);

        if (candidate.score < 0.0)
            std::printf ("  (reported)");
        else
            std::printf ("  %5.1f%%", 100.0 * candidate.score);

        if (candidate.halfOfPrimary)
            std::printf ("   (half of primary)");
        if (candidate.doubleOfPrimary)
            std::printf ("   (double of primary)");

        std::printf ("\n");
    }

    if (tempo.uncertain)
    {
        std::printf ("\n  The evidence is weak. Check the candidates above, or set the\n");
        std::printf ("  tempo by hand if you already know it.\n");
    }

    if (result.key.valid)
    {
        std::printf ("\nkey         : %s", result.key.name.c_str());
        if (result.key.uncertain)
            std::printf ("   [UNCERTAIN]");
        std::printf ("\n");
        std::printf ("confidence  : %.2f\n", result.key.confidence);

        if (result.key.tuningValid)
            std::printf ("tuning      : A = %.1f Hz  (%+.0f cents)\n",
                         result.key.referenceHz, result.key.tuningCents);

        std::printf ("\nalternatives\n");
        int shown = 0;
        for (const auto& candidate : result.key.candidates)
        {
            if (++shown > 3)
                break;
            std::printf ("  %-8s %5.1f%%\n", candidate.name.c_str(),
                         100.0 * candidate.score);
        }
    }
    else
    {
        std::printf ("\nkey         : no tonal centre detected\n");
    }

    printNoteValues (tmix::NoteTiming (tempo.bpm));

    printMixTiming (tmix::MixTiming (tempo.bpm));

    return 0;
}

} // namespace

#ifdef _WIN32

// Windows hands narrow argv over in the system ANSI code page, so any path
// containing non-ASCII characters arrives already mangled. Taking wargv
// instead keeps the whole path intact; the user's library is full of Chinese
// file names.
int wmain (int argc, wchar_t** argv)
{
    SetConsoleOutputCP (CP_UTF8);

    CommandLine command;
    for (int i = 1; i < argc; ++i)
    {
        const std::wstring argument (argv[i]);

        if (argument == L"--quiet" || argument == L"-q")
            command.quiet = true;
        else if (argument == L"--dump-onsets")
            command.dumpOnsets = true;
        else if (command.path.empty())
        {
            command.path = std::filesystem::path (argument);
            command.ok = true;
        }
    }

    return run (command);
}

#else

int main (int argc, char** argv)
{
    CommandLine command;
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument (argv[i]);

        if (argument == "--quiet" || argument == "-q")
            command.quiet = true;
        else if (argument == "--dump-onsets")
            command.dumpOnsets = true;
        else if (command.path.empty())
        {
            command.path = std::filesystem::path (argument);
            command.ok = true;
        }
    }

    return run (command);
}

#endif

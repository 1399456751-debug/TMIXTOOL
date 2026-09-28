#include "Analysis.h"

#include "AudioFile.h"
#include "Decimator.h"

#include <cstddef>
#include <vector>

namespace tmix {

namespace {

// Mono sum. The mid signal is preferred over a plain average because it keeps
// the centred elements - which is where the pulse lives - at full level while
// attenuating anything panned hard.
std::vector<float> toMono (const DecodedAudio& audio)
{
    const auto frameCount = static_cast<std::size_t> (audio.frameCount());
    std::vector<float> mono (frameCount, 0.0f);

    if (audio.channels == 1)
    {
        mono = audio.samples;
        mono.resize (frameCount);
        return mono;
    }

    const auto channels = static_cast<std::size_t> (audio.channels);

    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        double sum = 0.0;
        for (std::size_t channel = 0; channel < channels; ++channel)
            sum += audio.samples[frame * channels + channel];

        mono[frame] = static_cast<float> (sum / static_cast<double> (channels));
    }

    return mono;
}

bool report (const ProgressFn& onProgress, float fraction, const char* stage)
{
    return ! onProgress || onProgress (fraction, stage);
}

} // namespace

AnalysisResult analyseFile (const std::filesystem::path& path,
                            const AnalysisOptions& options,
                            const ProgressFn& onProgress)
{
    AnalysisResult result;

    if (! report (onProgress, 0.0f, "decoding"))
    {
        result.error = "cancelled";
        return result;
    }

    DecodedAudio audio;
    if (! decodeAudioFile (path, audio, result.error))
        return result;

    result.sampleRate      = audio.sampleRate;
    result.channels        = audio.channels;
    result.frameCount      = audio.frameCount();
    result.durationSeconds = audio.durationSeconds();
    result.levels = computeLevelStats (audio.samples.data(), audio.samples.size());

    if (audio.frameCount() == 0)
    {
        result.error = "file contains no audio";
        return result;
    }

    if (! report (onProgress, 0.2f, "preparing"))
    {
        result.error = "cancelled";
        return result;
    }

    const auto mono = toMono (audio);

    Decimator decimator (audio.sampleRate, options.analysisRate);
    const auto analysis = decimator.process (mono);

    if (! report (onProgress, 0.35f, "onset detection"))
    {
        result.error = "cancelled";
        return result;
    }

    const OnsetEnvelope envelope =
        computeOnsetEnvelope (analysis, decimator.outputRate(), options.onset);

    if (! report (onProgress, 0.7f, "tempo"))
    {
        result.error = "cancelled";
        return result;
    }

    result.tempo = estimateTempo (envelope, options.tempo);

    if (! report (onProgress, 0.85f, "key"))
    {
        result.error = "cancelled";
        return result;
    }

    // Key detection runs on the same decimated mono signal. It does not need
    // the full band, and reusing the buffer avoids a second decode.
    result.key = detectKey (analysis, decimator.outputRate(), options.key);

    if (! report (onProgress, 1.0f, "done"))
    {
        result.error = "cancelled";
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace tmix

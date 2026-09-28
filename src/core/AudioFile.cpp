#include "AudioFile.h"

#include "miniaudio_internal.h"

#include <cstddef>
#include <utility>

namespace tmix {

namespace {

// Upper bound on how much we will pre-allocate from a container's declared
// length. MP3 headers report an estimate that can be wildly optimistic, and a
// bogus value must not turn into a huge allocation.
constexpr std::size_t kMaxReserveBytes = 512u * 1024u * 1024u;

std::string describe (ma_result result)
{
    return ma_result_description (result);
}

} // namespace

bool decodeAudioFile (const std::filesystem::path& path,
                      DecodedAudio& out,
                      std::string& error)
{
    out = DecodedAudio{};
    error.clear();

    if (path.empty())
    {
        error = "no file path given";
        return false;
    }

    // A channel count and sample rate of zero mean "keep whatever the file
    // uses" - the analysis layer resamples on its own terms.
    ma_decoder_config config = ma_decoder_config_init (ma_format_f32, 0, 0);

    ma_decoder decoder;
    // The wide-character entry point is required: the library paths and the
    // user's music collection both contain non-ASCII characters.
    const ma_result initResult =
        ma_decoder_init_file_w (path.wstring().c_str(), &config, &decoder);

    if (initResult != MA_SUCCESS)
    {
        error = "cannot open audio file: " + describe (initResult);
        return false;
    }

    const ma_uint32 channels   = decoder.outputChannels;
    const ma_uint32 sampleRate = decoder.outputSampleRate;

    if (channels == 0 || sampleRate == 0)
    {
        ma_decoder_uninit (&decoder);
        error = "stream reports no channels or no sample rate";
        return false;
    }

    std::vector<float> samples;

    // Reserve from the declared length when it looks sane. This is only a
    // hint: the real frame count comes from what the decoder actually yields.
    ma_uint64 declaredFrames = 0;
    if (ma_decoder_get_length_in_pcm_frames (&decoder, &declaredFrames) == MA_SUCCESS
        && declaredFrames > 0)
    {
        const std::size_t bytes =
            static_cast<std::size_t> (declaredFrames) * channels * sizeof (float);
        if (bytes <= kMaxReserveBytes)
            samples.reserve (static_cast<std::size_t> (declaredFrames) * channels);
    }

    constexpr ma_uint64 kChunkFrames = 1u << 16;
    std::vector<float> chunk (static_cast<std::size_t> (kChunkFrames) * channels);

    for (;;)
    {
        ma_uint64 framesRead = 0;
        const ma_result readResult =
            ma_decoder_read_pcm_frames (&decoder, chunk.data(), kChunkFrames, &framesRead);

        if (readResult != MA_SUCCESS && readResult != MA_AT_END)
        {
            ma_decoder_uninit (&decoder);
            error = "error while decoding: " + describe (readResult);
            return false;
        }

        if (framesRead > 0)
        {
            const std::size_t count = static_cast<std::size_t> (framesRead) * channels;
            samples.insert (samples.end(), chunk.begin(),
                            chunk.begin() + static_cast<std::ptrdiff_t> (count));
        }

        if (framesRead < kChunkFrames)
            break; // Short read means the end of the stream.
    }

    ma_decoder_uninit (&decoder);

    out.sampleRate = static_cast<int> (sampleRate);
    out.channels   = static_cast<int> (channels);
    out.samples    = std::move (samples);
    return true;
}

} // namespace tmix

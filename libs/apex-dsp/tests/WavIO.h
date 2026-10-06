#pragma once

// Minimal WAV reader / writer for the offline tools (no JUCE).
// Reads 16 / 24 / 32-bit PCM and 32-bit float; writes 32-bit float.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace apex::test
{

struct Audio
{
    double sampleRate = 48000.0;
    std::vector<std::vector<float>> channels;

    int numChannels() const { return (int) channels.size(); }
    int numSamples() const { return channels.empty() ? 0 : (int) channels[0].size(); }
};

inline bool readWav (const std::string& path, Audio& out)
{
    std::ifstream f (path, std::ios::binary);
    if (! f)
        return false;

    std::vector<char> bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12 || std::memcmp (bytes.data(), "RIFF", 4) != 0 || std::memcmp (bytes.data() + 8, "WAVE", 4) != 0)
        return false;

    auto u16 = [&] (size_t o) { std::uint16_t v; std::memcpy (&v, bytes.data() + o, 2); return v; };
    auto u32 = [&] (size_t o) { std::uint32_t v; std::memcpy (&v, bytes.data() + o, 4); return v; };

    int format = 0, channels = 0, bits = 0;
    size_t dataOffset = 0, dataSize = 0;

    for (size_t pos = 12; pos + 8 <= bytes.size();)
    {
        const std::uint32_t size = u32 (pos + 4);
        if (std::memcmp (bytes.data() + pos, "fmt ", 4) == 0)
        {
            format         = u16 (pos + 8);
            channels       = u16 (pos + 10);
            out.sampleRate = u32 (pos + 12);
            bits           = u16 (pos + 22);
            if (format == 0xFFFE && size >= 40)
                format = u16 (pos + 32);   // WAVE_FORMAT_EXTENSIBLE sub-format
        }
        else if (std::memcmp (bytes.data() + pos, "data", 4) == 0)
        {
            dataOffset = pos + 8;
            dataSize   = std::min<size_t> (size, bytes.size() - dataOffset);
        }
        pos += 8 + size + (size & 1);
    }

    if (channels <= 0 || dataOffset == 0 || (format != 1 && format != 3))
        return false;

    const int bytesPerSample = bits / 8;
    const size_t frames = dataSize / (size_t) (bytesPerSample * channels);
    out.channels.assign ((size_t) channels, std::vector<float> (frames));

    for (size_t i = 0; i < frames; ++i)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            const char* p = bytes.data() + dataOffset + (i * (size_t) channels + (size_t) ch) * (size_t) bytesPerSample;
            float v = 0.0f;
            if (format == 3 && bits == 32)      { std::memcpy (&v, p, 4); }
            else if (bits == 16)                { std::int16_t s; std::memcpy (&s, p, 2); v = (float) s / 32768.0f; }
            else if (bits == 24)                { std::int32_t s = (std::uint8_t) p[0] | ((std::uint8_t) p[1] << 8) | ((std::int8_t) p[2] << 16); v = (float) s / 8388608.0f; }
            else if (bits == 32)                { std::int32_t s; std::memcpy (&s, p, 4); v = (float) ((double) s / 2147483648.0); }
            else return false;
            out.channels[(size_t) ch][i] = v;
        }
    }
    return true;
}

inline bool writeWav (const std::string& path, const Audio& audio)
{
    std::ofstream f (path, std::ios::binary);
    if (! f)
        return false;

    const std::uint16_t channels = (std::uint16_t) audio.numChannels();
    const std::uint32_t frames   = (std::uint32_t) audio.numSamples();
    const std::uint32_t rate     = (std::uint32_t) audio.sampleRate;
    const std::uint32_t dataSize = frames * channels * 4u;

    auto w16 = [&] (std::uint16_t v) { f.write ((const char*) &v, 2); };
    auto w32 = [&] (std::uint32_t v) { f.write ((const char*) &v, 4); };

    f.write ("RIFF", 4); w32 (36 + dataSize); f.write ("WAVE", 4);
    f.write ("fmt ", 4); w32 (16); w16 (3); w16 (channels); w32 (rate);
    w32 (rate * channels * 4u); w16 ((std::uint16_t) (channels * 4)); w16 (32);
    f.write ("data", 4); w32 (dataSize);

    for (std::uint32_t i = 0; i < frames; ++i)
        for (std::uint16_t ch = 0; ch < channels; ++ch)
            f.write ((const char*) &audio.channels[ch][i], 4);

    return (bool) f;
}

} // namespace apex::test

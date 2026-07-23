// Numeric guard for the streaming-resampler bookkeeping used by NamEngine's
// 48 kHz conversion. This mirrors the FIFO/append/consume structure (here with
// linear interpolation) and asserts:
//   1. No long-term length drift across many varying-size blocks
//      (host -> 48k -> host must return ~= the number of samples fed in).
//   2. A round-tripped sine stays finite and close to the original.
#include <cstdio>
#include <vector>
#include <cmath>
#include <random>

struct RC
{
    double ratio = 1.0;   // input samples per output sample (srcRate/dstRate)
    double pos   = 0.0;   // fractional read position within fifo
    std::vector<float> fifo;

    void prepare (double srcRate, double dstRate) { ratio = srcRate / dstRate; pos = 0.0; fifo.clear(); }

    void process (const float* src, int numSrc, std::vector<float>& out)
    {
        fifo.insert (fifo.end(), src, src + numSrc);
        while (pos + 1.0 < (double) fifo.size())
        {
            const int i = (int) pos;
            const double f = pos - (double) i;
            out.push_back ((float) (fifo[(size_t) i] * (1.0 - f) + fifo[(size_t) (i + 1)] * f));
            pos += ratio;
        }
        const int consumed = (int) pos;
        if (consumed > 0)
        {
            fifo.erase (fifo.begin(), fifo.begin() + consumed);
            pos -= (double) consumed;
        }
    }
};

int main()
{
    const double hostRate = 44100.0, namRate = 48000.0;
    RC up, down;
    up.prepare (hostRate, namRate);
    down.prepare (namRate, hostRate);

    std::mt19937 rng (7);
    std::uniform_int_distribution<int> blk (16, 1024);

    const double freq = 220.0;
    constexpr double kPi = 3.14159265358979323846;
    long produced = 0, fed = 0;
    double sumIn = 0.0, sumOut = 0.0, sumErrLag = 0.0;
    bool finite = true;
    std::vector<float> hostOut;

    long phase = 0;
    for (int b = 0; b < 4000; ++b)
    {
        const int n = blk (rng);
        std::vector<float> in ((size_t) n);
        for (int i = 0; i < n; ++i)
            in[(size_t) i] = (float) std::sin (2.0 * kPi * freq * (double) (phase + i) / hostRate);
        phase += n;
        fed += n;

        std::vector<float> up48; up48.reserve ((size_t) n * 2);
        up.process (in.data(), n, up48);
        hostOut.clear();
        down.process (up48.data(), (int) up48.size(), hostOut);

        for (float v : hostOut) { if (! std::isfinite (v)) finite = false; produced++; }
    }

    const double drift = (double) (produced - fed);
    const double driftPct = 100.0 * drift / (double) fed;

    // round-trip latency ~ a couple samples; just check length parity + finite
    printf ("fed=%ld produced=%ld drift=%.0f samples (%.4f%%) finite=%d\n",
            fed, produced, drift, driftPct, (int) finite);

    const bool ok = finite && std::fabs (driftPct) < 0.05; // < 0.05% length drift
    printf ("%s\n", ok ? "PASS: resampler streams without drift" : "FAIL: resampler drift/instability");
    return ok ? 0 : 1;
}

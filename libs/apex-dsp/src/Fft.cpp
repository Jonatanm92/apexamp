#include "apex/dsp/Fft.h"

#include <cmath>
#include <utility>

namespace apex::dsp
{

void Fft::prepare (int order)
{
    n = 1 << order;

    bitReverse.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        int r = 0;
        for (int b = 0; b < order; ++b)
            if (i & (1 << b))
                r |= 1 << (order - 1 - b);
        bitReverse[(size_t) i] = r;
    }

    twiddles.resize ((size_t) (n / 2));
    constexpr double twoPi = 6.283185307179586476925286766559;
    for (int k = 0; k < n / 2; ++k)
    {
        const double a = -twoPi * (double) k / (double) n;
        twiddles[(size_t) k] = { (float) std::cos (a), (float) std::sin (a) };
    }
}

void Fft::forward (Complex* data) const noexcept
{
    transform (data, false);
}

void Fft::inverse (Complex* data) const noexcept
{
    transform (data, true);

    const float scale = 1.0f / (float) n;
    for (int i = 0; i < n; ++i)
        data[i] *= scale;
}

void Fft::transform (Complex* data, bool inverse) const noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const int j = bitReverse[(size_t) i];
        if (i < j)
            std::swap (data[i], data[j]);
    }

    // Complex multiplies are written out by hand: std::complex<float>::operator*
    // goes through the slow NaN-checking __mulsc3 path without -ffast-math.
    const float sign = inverse ? -1.0f : 1.0f;

    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len / 2;
        const int step = n / len;

        for (int i = 0; i < n; i += len)
        {
            for (int k = 0; k < half; ++k)
            {
                const Complex w = twiddles[(size_t) (k * step)];
                const float wr = w.real();
                const float wi = sign * w.imag();

                const Complex v = data[i + k + half];
                const float vr = v.real() * wr - v.imag() * wi;
                const float vi = v.real() * wi + v.imag() * wr;

                const Complex u = data[i + k];
                data[i + k]        = { u.real() + vr, u.imag() + vi };
                data[i + k + half] = { u.real() - vr, u.imag() - vi };
            }
        }
    }
}

} // namespace apex::dsp

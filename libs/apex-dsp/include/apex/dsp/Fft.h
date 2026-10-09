#pragma once

#include <complex>
#include <vector>

namespace apex::dsp
{

/**
 * Fft
 * ---
 * In-place iterative radix-2 complex FFT. The bit-reversal and twiddle tables
 * are built in prepare(), so forward() / inverse() never allocate and are safe
 * to call from the audio thread.
 */
class Fft
{
public:
    using Complex = std::complex<float>;

    /** Builds the tables for a 2^order point transform (allocates). */
    void prepare (int order);

    int size() const noexcept { return n; }

    /** Forward transform, unscaled. */
    void forward (Complex* data) const noexcept;

    /** Inverse transform, scaled by 1/N so inverse (forward (x)) == x. */
    void inverse (Complex* data) const noexcept;

private:
    void transform (Complex* data, bool inverse) const noexcept;

    int n = 0;
    std::vector<int> bitReverse;
    std::vector<Complex> twiddles;   // e^(-2 pi i k / N), k < N/2
};

} // namespace apex::dsp

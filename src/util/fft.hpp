// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace runeharbor::util
{

/**
 * Fast Fourier Transform (FFT) and Related Transforms
 *
 * Optimized for performance using O(N log N) algorithms.
 * Replaces the placeholder O(N^2) implementations.
 */
class FFT
{
  public:
    /**
     * Real Discrete Fourier Transform (RDFT)
     *
     * Data layout (n must be power of 2):
     * [DC, Real1, Imag1, Real2, Imag2, ..., Nyquist]
     *
     * @param data Input/Output data span
     * @param inverse If true, performs inverse transform
     */
    static void rdft(std::span<float> data, bool inverse);

    /**
     * Discrete Cosine Transform (DCT-II/III)
     *
     * @param data Input/Output data span
     * @param inverse If true, performs inverse transform (DCT-III)
     */
    static void dct(std::span<float> data, bool inverse);

  private:
    static void computeFft(float* real, float* imag, size_t n, bool inverse);
    static void bitReverse(float* real, float* imag, size_t n);
};

} // namespace runeharbor::util

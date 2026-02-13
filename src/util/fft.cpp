// SPDX-License-Identifier: MIT
#include "fft.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace runeharbor::util
{

void FFT::rdft(std::span<float> data, bool inverse)
{
    const size_t n = data.size();
    if (n < 2 || (n & (n - 1)) != 0)
    {
        return;
    }

    size_t halfN = n / 2;
    std::vector<float> real(n, 0.0f);
    std::vector<float> imag(n, 0.0f);

    if (inverse)
    {
        real[0] = data[0];
        for (size_t k = 1; k < halfN; k++)
        {
            real[k] = data[k * 2 - 1];
            imag[k] = -data[k * 2];
            real[n - k] = real[k];
            imag[n - k] = -imag[k];
        }
        if (n > 1)
        {
            real[halfN] = data[n - 1];
        }

        computeFft(real.data(), imag.data(), n, true);
        for (size_t i = 0; i < n; i++)
        {
            data[i] = real[i];
        }
    }
    else
    {
        for (size_t i = 0; i < n; i++)
        {
            real[i] = data[i];
        }
        computeFft(real.data(), imag.data(), n, false);

        data[0] = real[0];
        for (size_t k = 1; k < halfN; k++)
        {
            data[k * 2 - 1] = real[k];
            data[k * 2] = -imag[k];
        }
        data[n - 1] = real[halfN];
    }
}

void FFT::dct(std::span<float> data, bool inverse)
{
    const size_t n = data.size();
    if (n == 0)
    {
        return;
    }
    if (n == 1)
    {
        return;
    }

    // O(N log N) DCT using FFT
    // For power-of-two N, we can use the following algorithm:
    if ((n & (n - 1)) == 0)
    {
        if (inverse)
        {
            // DCT-III (Inverse)
            std::vector<float> real(n);
            std::vector<float> imag(n);

            constexpr float kPi = std::numbers::pi_v<float>;
            
            real[0] = data[0];
            for (size_t i = 1; i < n; i++)
            {
                float angle = -kPi * static_cast<float>(i) / (2.0f * n);
                real[i] = data[i] * std::cos(angle);
                imag[i] = data[i] * std::sin(angle);
            }

            computeFft(real.data(), imag.data(), n, true);

            for (size_t i = 0; i < n / 2; i++)
            {
                data[i * 2] = real[i] * 2.0f;
                data[i * 2 + 1] = real[n - 1 - i] * 2.0f;
            }
            
            // Re-apply normalization for DCT
            float scale = 1.0f / std::sqrt(static_cast<float>(n));
            for (size_t i = 0; i < n; i++)
            {
                data[i] *= scale;
            }
        }
        else
        {
            // DCT-II (Forward)
            std::vector<float> real(n);
            std::vector<float> imag(n, 0.0f);

            for (size_t i = 0; i < n / 2; i++)
            {
                real[i] = data[i * 2];
                real[n - 1 - i] = data[i * 2 + 1];
            }

            computeFft(real.data(), imag.data(), n, false);

            constexpr float kPi = std::numbers::pi_v<float>;
            float scale = std::sqrt(2.0f / static_cast<float>(n));
            
            data[0] = real[0] * scale / std::sqrt(2.0f);
            for (size_t k = 1; k < n; k++)
            {
                float angle = -kPi * static_cast<float>(k) / (2.0f * n);
                data[k] = (real[k] * std::cos(angle) - imag[k] * std::sin(angle)) * scale;
            }
        }
        return;
    }

    // Fallback to O(N^2) for non-power-of-two (though Bink/Smacker use power-of-two)
    std::vector<float> temp(data.begin(), data.end());
    constexpr float kPi = std::numbers::pi_v<float>;

    if (inverse)
    {
        // DCT-III (Inverse DCT)
        float scale = 2.0f / static_cast<float>(n);
        for (size_t i = 0; i < n; i++)
        {
            float sum = temp[0] / 2.0f;
            for (size_t k = 1; k < n; k++)
            {
                float angle =
                    kPi * static_cast<float>(k) * (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
                sum += temp[k] * std::cos(angle);
            }
            data[i] = sum * scale;
        }
    }
    else
    {
        // DCT-II (Forward DCT)
        float scale = std::sqrt(2.0f / static_cast<float>(n));
        for (size_t k = 0; k < n; k++)
        {
            float sum = 0.0f;
            for (size_t i = 0; i < n; i++)
            {
                float angle =
                    kPi * static_cast<float>(k) * (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
                sum += temp[i] * std::cos(angle);
            }
            data[k] = sum * scale;
            if (k == 0)
            {
                data[k] /= std::sqrt(2.0f);
            }
        }
    }
}

void FFT::computeFft(float* real, float* imag, size_t n, bool inverse)
{
    bitReverse(real, imag, n);

    for (size_t len = 2; len <= n; len <<= 1)
    {
        float angle =
            2.0f * std::numbers::pi_v<float> / static_cast<float>(len) * (inverse ? -1.0f : 1.0f);
        float wlen_r = std::cos(angle);
        float wlen_i = std::sin(angle);

        for (size_t i = 0; i < n; i += len)
        {
            float w_r = 1.0f;
            float w_i = 0.0f;
            for (size_t j = 0; j < len / 2; j++)
            {
                float u_r = real[i + j];
                float u_i = imag[i + j];
                float v_r = real[i + j + len / 2] * w_r - imag[i + j + len / 2] * w_i;
                float v_i = real[i + j + len / 2] * w_i + imag[i + j + len / 2] * w_r;
                real[i + j] = u_r + v_r;
                imag[i + j] = u_i + v_i;
                real[i + j + len / 2] = u_r - v_r;
                imag[i + j + len / 2] = u_i - v_i;

                float next_w_r = w_r * wlen_r - w_i * wlen_i;
                w_i = w_r * wlen_i + w_i * wlen_r;
                w_r = next_w_r;
            }
        }
    }

    if (inverse)
    {
        for (size_t i = 0; i < n; i++)
        {
            real[i] /= static_cast<float>(n);
            imag[i] /= static_cast<float>(n);
        }
    }
}

void FFT::bitReverse(float* real, float* imag, size_t n)
{
    for (size_t i = 1, j = 0; i < n; i++)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
        {
            j ^= bit;
        }
        j ^= bit;

        if (i < j)
        {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }
}

} // namespace runeharbor::util

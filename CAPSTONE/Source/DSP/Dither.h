#pragma once
// ============================================================================
//  CAPSTONE — Dither et mise en forme du bruit
//
//  Toujours en tout dernier, après le limiteur, et une seule fois dans toute
//  la chaîne de production. Réduire 24 vers 16 bits sans dither produit une
//  distorsion de troncature corrélée au signal — bien plus audible sur les
//  fins de réverbération que le bruit qu'on cherche à éviter.
// ============================================================================
#include "Ballistics.h"
#include <array>
#include <cstdint>

namespace mono {

enum class DitherMode { Off = 0, TpdfFlat = 1, ShapedLight = 2, ShapedStrong = 3 };

class Dither
{
public:
    void prepare (int numCh) noexcept
    {
        nCh = std::min (numCh, 2);
        for (size_t c = 0; c < 2; ++c) { hist[c].fill (0.0); state[c] = 0x9E3779B9u + (uint32_t) c * 0x85EBCA6Bu; }
    }

    /** bits : 16, 20 ou 24. mode Off => passthrough. */
    void setParams (DitherMode m, int bits_) noexcept
    {
        mode = m;
        bits = clampi (bits_, 8, 24);
        q    = 1.0 / std::pow (2.0, bits - 1);
    }

    void process (float* const* io, int numCh, int n) noexcept
    {
        if (mode == DitherMode::Off) return;
        const int ch = std::min (numCh, nCh);

        // Courbes pondérées psychoacoustiquement : repoussent le bruit hors de
        // la zone 2–5 kHz où l'oreille est la plus sensible.
        static const double light[3]  = { 1.623, -0.982,  0.109 };
        static const double strong[9] = { 2.412, -3.370,  3.937, -4.174,  3.353,
                                         -2.205,  1.281, -0.569,  0.0847 };
        const double* h    = (mode == DitherMode::ShapedStrong) ? strong
                           : (mode == DitherMode::ShapedLight)  ? light : nullptr;
        const int     hLen = (mode == DitherMode::ShapedStrong) ? 9
                           : (mode == DitherMode::ShapedLight)  ? 3 : 0;

        for (int c = 0; c < ch; ++c)
        {
            auto& e = hist[(size_t) c];
            for (int i = 0; i < n; ++i)
            {
                double fb = 0.0;
                for (int k = 0; k < hLen; ++k) fb += h[k] * e[(size_t) k];

                const double s = (double) io[c][i] - fb;
                const double d = (uniform (c) + uniform (c) - 1.0) * q;   // TPDF ±1 LSB
                const double y = std::round ((s + d) / q) * q;

                for (int k = 8; k > 0; --k) e[(size_t) k] = e[(size_t) (k - 1)];
                e[0] = y - (s + d);                                       // erreur de quantification

                io[c][i] = (float) clampf ((float) y, -1.0f, 1.0f);
            }
        }
    }

private:
    static int clampi (int v, int lo, int hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

    /** xorshift32 : rapide, déterministe, sans allocation ni verrou. */
    inline double uniform (int c) noexcept
    {
        uint32_t& x = state[(size_t) c];
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return (double) x * (1.0 / 4294967296.0);
    }

    std::array<std::array<double, 9>, 2> hist {};
    uint32_t state[2] { 1u, 2u };
    DitherMode mode = DitherMode::Off;
    int nCh = 2, bits = 16;
    double q = 1.0 / 32768.0;
};

} // namespace mono

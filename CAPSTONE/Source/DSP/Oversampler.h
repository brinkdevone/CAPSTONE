#pragma once
// ============================================================================
//  CAPSTONE — Suréchantillonnage en cascade ×2 / ×4 / ×8
//
//  Pourquoi plusieurs facteurs : un écrêtage franc génère des harmoniques
//  d'ordre très élevé. En ×2 (96 kHz), la 11e et la 13e harmonique d'un 7 kHz
//  se replient déjà SOUS la fréquence de coupure du filtre de décimation —
//  elles passent donc intactes dans la bande audible. Il faut ×4 minimum pour
//  qu'un clipper soit propre ; ×2 suffit pour une saturation douce.
// ============================================================================
#include "Biquad.h"
#include <vector>

namespace mono {

class Oversampler
{
public:
    static constexpr int kMaxStages = 3;   // ×8 au maximum

    /** stages : 1 => ×2, 2 => ×4, 3 => ×8. */
    void prepare (double baseSr, int maxBlock, int stages_)
    {
        stages = clampi (stages_, 0, kMaxStages);
        baseRate = baseSr;
        // Q Butterworth 8e ordre
        static const double q[] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };

        for (int s = 0; s < stages; ++s)
        {
            const double inRate  = baseSr * std::pow (2.0, s);
            const double outRate = inRate * 2.0;
            const double cutoff  = inRate * 0.45;
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < 4; ++k)
                {
                    up  [s][c][k].makeLowpass (outRate, cutoff, q[k]);
                    down[s][c][k].makeLowpass (outRate, cutoff, q[k]);
                }
            buf[s].assign ((size_t) maxBlock * (size_t) (1 << (s + 1)) + 8, 0.0f);
        }
        reset();
    }

    void reset() noexcept
    {
        for (int s = 0; s < kMaxStages; ++s)
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < 4; ++k) { up[s][c][k].reset(); down[s][c][k].reset(); }
    }

    int factor() const noexcept { return 1 << stages; }

    /** Renvoie un pointeur sur n*factor() échantillons suréchantillonnés. */
    float* upsample (int ch, const float* in, int n) noexcept
    {
        if (stages == 0) return nullptr;
        const float* src = in;
        int len = n;
        for (int s = 0; s < stages; ++s)
        {
            float* dst = buf[s].data();
            for (int i = 0; i < len; ++i)
            {
                float a = src[i] * 2.0f, z = 0.0f;   // ×2 compense l'insertion de zéros
                for (int k = 0; k < 4; ++k) a = up[s][ch][k].process (a);
                for (int k = 0; k < 4; ++k) z = up[s][ch][k].process (z);
                dst[2 * i] = a; dst[2 * i + 1] = z;
            }
            src = dst; len *= 2;
        }
        return buf[stages - 1].data();
    }

    /** Filtre puis décime en cascade inverse. `os` doit être le buffer rendu par upsample. */
    void downsample (int ch, float* os, float* out, int n) noexcept
    {
        if (stages == 0) return;
        int len = n * (1 << stages);
        for (int s = stages - 1; s >= 0; --s)
        {
            float* dst = (s == 0) ? out : buf[s - 1].data();
            const int outLen = len / 2;
            for (int i = 0; i < outLen; ++i)
            {
                float a = os[2 * i], z = os[2 * i + 1];
                for (int k = 0; k < 4; ++k) a = down[s][ch][k].process (a);
                for (int k = 0; k < 4; ++k) z = down[s][ch][k].process (z);
                dst[i] = a;
            }
            os = dst; len = outLen;
        }
    }

private:
    static int clampi (int v, int lo, int hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

    Biquad up[kMaxStages][2][4], down[kMaxStages][2][4];
    std::vector<float> buf[kMaxStages];
    double baseRate = 48000.0;
    int stages = 1;
};

} // namespace mono

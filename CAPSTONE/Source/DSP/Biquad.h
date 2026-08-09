#pragma once
// ============================================================================
//  MONOLITH — Biquads RBJ + filtres à pente variable + Linkwitz-Riley 4
// ============================================================================
#include "Ballistics.h"

namespace mono {

/** Biquad Transposed Direct Form II (stable, faible bruit). */
struct Biquad
{
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
    float z1 = 0.f, z2 = 0.f;

    void reset() noexcept { z1 = z2 = 0.f; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setCoefs (double bb0, double bb1, double bb2, double aa0, double aa1, double aa2) noexcept
    {
        const double inv = 1.0 / aa0;
        b0 = (float) (bb0 * inv); b1 = (float) (bb1 * inv); b2 = (float) (bb2 * inv);
        a1 = (float) (aa1 * inv); a2 = (float) (aa2 * inv);
    }

    // ---- Cookbook RBJ -------------------------------------------------------
    void makeLowpass (double sr, double f, double Q) noexcept
    {
        const double w = 6.283185307179586 * clampD (f, sr), c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * std::max (0.05, Q));
        setCoefs ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + al, -2 * c, 1 - al);
    }
    void makeHighpass (double sr, double f, double Q) noexcept
    {
        const double w = 6.283185307179586 * clampD (f, sr), c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * std::max (0.05, Q));
        setCoefs ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + al, -2 * c, 1 - al);
    }
    void makeBandpass (double sr, double f, double Q) noexcept
    {
        const double w = 6.283185307179586 * clampD (f, sr), c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * std::max (0.05, Q));
        setCoefs (al, 0.0, -al, 1 + al, -2 * c, 1 - al);
    }
    void makePeak (double sr, double f, double Q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 6.283185307179586 * clampD (f, sr), c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * std::max (0.05, Q));
        setCoefs (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void makeLowShelf (double sr, double f, double Q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
        const double w = 6.283185307179586 * clampD (f, sr), c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * std::max (0.05, Q));
        setCoefs (A * ((A + 1) - (A - 1) * c + 2 * sA * al),
                  2 * A * ((A - 1) - (A + 1) * c),
                  A * ((A + 1) - (A - 1) * c - 2 * sA * al),
                  (A + 1) + (A - 1) * c + 2 * sA * al,
                  -2 * ((A - 1) + (A + 1) * c),
                  (A + 1) + (A - 1) * c - 2 * sA * al);
    }
    void makeHighShelf (double sr, double f, double Q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
        const double w = 6.283185307179586 * clampD (f, sr), c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * std::max (0.05, Q));
        setCoefs (A * ((A + 1) + (A - 1) * c + 2 * sA * al),
                  -2 * A * ((A - 1) + (A + 1) * c),
                  A * ((A + 1) + (A - 1) * c - 2 * sA * al),
                  (A + 1) - (A - 1) * c + 2 * sA * al,
                  2 * ((A - 1) - (A + 1) * c),
                  (A + 1) - (A - 1) * c - 2 * sA * al);
    }
    /** Passe-tout 2e ordre : utilisé pour recaler la phase entre bandes d'un
        crossover multibande (deux en cascade = passe-tout Linkwitz-Riley 4). */
    void makeAllpass (double sr, double f, double Q) noexcept
    {
        const double w = 6.283185307179586 * clampD (f, sr), c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * std::max (0.05, Q));
        setCoefs (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
    }
    /** Coefficients bruts (pour les filtres normalisés type K-weighting). */
    void setDirect (double bb0, double bb1, double bb2, double aa1, double aa2) noexcept
    { b0 = (float) bb0; b1 = (float) bb1; b2 = (float) bb2; a1 = (float) aa1; a2 = (float) aa2; }

    void makeBypass() noexcept { b0 = 1.f; b1 = b2 = a1 = a2 = 0.f; }

private:
    static double clampD (double f, double sr) noexcept
    { return std::min (std::max (f, 5.0), sr * 0.49) / sr; }
};

/** Filtre 1er ordre (6 dB/oct) bilinéaire. */
struct OnePole
{
    float b0 = 1.f, b1 = 0.f, a1 = 0.f, z = 0.f;
    void reset() noexcept { z = 0.f; }
    inline float process (float x) noexcept { const float y = b0 * x + z; z = b1 * x - a1 * y; return y; }
    void makeLowpass (double sr, double f) noexcept
    {
        const double K = std::tan (3.141592653589793 * std::min (f, sr * 0.49) / sr), n = 1.0 / (1.0 + K);
        b0 = (float) (K * n); b1 = (float) (K * n); a1 = (float) ((K - 1.0) * n);
    }
    void makeHighpass (double sr, double f) noexcept
    {
        const double K = std::tan (3.141592653589793 * std::min (f, sr * 0.49) / sr), n = 1.0 / (1.0 + K);
        b0 = (float) n; b1 = (float) -n; a1 = (float) ((K - 1.0) * n);
    }
    void makeBypass() noexcept { b0 = 1.f; b1 = 0.f; a1 = 0.f; }
};

/** Coupe-bande à pente sélectionnable : 6 / 12 / 18 / 24 / 36 dB/oct (Butterworth). */
class SlopeFilter
{
public:
    void prepare (int numChannels) noexcept { nCh = std::min (numChannels, 2); reset(); }
    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c) { p1[c].reset(); for (auto& b : bq[c]) b.reset(); }
    }

    /** slopeIdx : 0=6, 1=12, 2=18, 3=24, 4=36 dB/oct. */
    void setup (double sr, bool highpass, double freq, int slopeIdx, bool active) noexcept
    {
        enabled = active;
        stages  = 0; useOnePole = false;
        if (! active) return;

        // Q Butterworth par étage
        static const double q2[]  = { 0.70710678 };
        static const double q4[]  = { 0.54119610, 1.30656296 };
        static const double q6[]  = { 0.51763809, 0.70710678, 1.93185165 };

        for (int c = 0; c < 2; ++c)
        {
            switch (slopeIdx)
            {
                case 0: useOnePole = true; stages = 0;
                    highpass ? p1[c].makeHighpass (sr, freq) : p1[c].makeLowpass (sr, freq); break;
                case 1: stages = 1;
                    highpass ? bq[c][0].makeHighpass (sr, freq, q2[0]) : bq[c][0].makeLowpass (sr, freq, q2[0]); break;
                case 2: useOnePole = true; stages = 1;
                    highpass ? p1[c].makeHighpass (sr, freq) : p1[c].makeLowpass (sr, freq);
                    highpass ? bq[c][0].makeHighpass (sr, freq, 1.0) : bq[c][0].makeLowpass (sr, freq, 1.0); break;
                case 3: stages = 2;
                    for (int i = 0; i < 2; ++i)
                        highpass ? bq[c][i].makeHighpass (sr, freq, q4[i]) : bq[c][i].makeLowpass (sr, freq, q4[i]);
                    break;
                default: stages = 3;
                    for (int i = 0; i < 3; ++i)
                        highpass ? bq[c][i].makeHighpass (sr, freq, q6[i]) : bq[c][i].makeLowpass (sr, freq, q6[i]);
                    break;
            }
        }
    }

    inline float process (int ch, float x) noexcept
    {
        if (! enabled) return x;
        if (useOnePole) x = p1[ch].process (x);
        for (int i = 0; i < stages; ++i) x = bq[ch][i].process (x);
        return x;
    }

private:
    Biquad bq[2][3];
    OnePole p1[2];
    int nCh = 2, stages = 0;
    bool useOnePole = false, enabled = false;
};

/** Passe-tout équivalent à la somme d'un crossover Linkwitz-Riley 4.

    Démonstration : LP4 = LP2², HP4 = HP2² avec LP2 = wc²/D, HP2 = s²/D et
    D = s² + V2·wc·s + wc². Alors
        LP4 + HP4 = (s⁴ + wc⁴) / D²  et  s⁴ + wc⁴ = D · (s² - V2·wc·s + wc²)
    donc LP4 + HP4 = (s² - V2·wc·s + wc²) / (s² + V2·wc·s + wc²) :
    un passe-tout du SECOND ordre, pas du quatrième. Mettre deux sections en
    cascade doublerait la phase et rendrait la compensation fausse. */
class AllpassLR4
{
public:
    void reset() noexcept { for (int c = 0; c < 2; ++c) ap[c].reset(); }
    void setFreq (double sr, double f) noexcept
    { for (int c = 0; c < 2; ++c) ap[c].makeAllpass (sr, f, 0.70710678); }
    inline float process (int ch, float x) noexcept { return ap[ch].process (x); }
private:
    Biquad ap[2];
};

/** Linkwitz-Riley 4e ordre : LP + HP se somment à plat et en phase. */
class LinkwitzRiley4
{
public:
    void reset() noexcept { for (int c = 0; c < 2; ++c) for (int i = 0; i < 2; ++i) { lp[c][i].reset(); hp[c][i].reset(); } }
    void setCrossover (double sr, double f) noexcept
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 2; ++i) { lp[c][i].makeLowpass (sr, f, 0.70710678); hp[c][i].makeHighpass (sr, f, 0.70710678); }
    }
    inline void process (int ch, float x, float& low, float& high) noexcept
    {
        low  = lp[ch][1].process (lp[ch][0].process (x));
        high = hp[ch][1].process (hp[ch][0].process (x));
    }
private:
    Biquad lp[2][2], hp[2][2];
};

} // namespace mono

#pragma once
// ============================================================================
//  CAPSTONE — Égaliseur dynamique (2 bandes)
//
//  L'outil qui distingue le mastering du mixage : un mix correct au refrain
//  mais bouché au couplet ne se corrige pas avec une courbe statique. Ici la
//  cloche ne se déclenche que quand la bande dépasse le seuil.
// ============================================================================
#include "Biquad.h"

namespace mono {

class DynamicBand
{
public:
    void prepare (double sampleRate, int numCh) noexcept
    {
        sr = sampleRate; nCh = std::min (numCh, 2);
        for (int c = 0; c < 2; ++c) { det[c].reset(); bell[c].makeBypass(); }
        env = 0.0f; curGainDb = 0.0f; counter = 0;
    }

    /** rangeDb > 0 => la bande atténue quand ça dépasse ; < 0 => elle renforce. */
    void setParams (bool on, float freq_, float q_, float threshDb_, float ratio_,
                    float attMs, float relMs, float rangeDb_) noexcept
    {
        enabled = on; freq = freq_; q = q_;
        threshDb = threshDb_; ratio = std::max (1.0f, ratio_); rangeDb = rangeDb_;
        aCoef = timeCoef (attMs, sr); rCoef = timeCoef (relMs, sr);
        for (int c = 0; c < 2; ++c) det[c].makeBandpass (sr, freq, std::max (0.5f, q));
    }

    void process (float* const* io, int numCh, int n) noexcept
    {
        if (! enabled) { curGainDb = 0.0f; return; }
        const int ch = std::min (numCh, nCh);

        for (int i = 0; i < n; ++i)
        {
            float d = 0.0f;
            for (int c = 0; c < ch; ++c) d = std::max (d, std::abs (det[c].process (io[c][i])));

            const float c2 = (d > env) ? aCoef : rCoef;
            env = c2 * env + (1.0f - c2) * d;

            const float over = gainToDb (env) - threshDb;
            float target = 0.0f;
            if (over > 0.0f)
            {
                const float amount = over * (1.0f - 1.0f / ratio);
                target = (rangeDb >= 0.0f) ? -std::min (amount, rangeDb)
                                           :  std::min (amount, -rangeDb);
            }
            // Lissage + recalcul périodique des coefficients : recalculer un
            // biquad à chaque échantillon coûterait cher pour un gain inaudible.
            curGainDb += (target - curGainDb) * 0.002f;
            if (--counter <= 0)
            {
                counter = 16;
                for (int c = 0; c < ch; ++c) bell[c].makePeak (sr, freq, q, curGainDb);
            }
            for (int c = 0; c < ch; ++c) io[c][i] = bell[c].process (io[c][i]);
        }
    }

    float getGainDb() const noexcept { return curGainDb; }

private:
    double sr = 48000.0; int nCh = 2;
    Biquad det[2], bell[2];
    bool enabled = false;
    float freq = 1000.f, q = 1.f, threshDb = -20.f, ratio = 3.f, rangeDb = 6.f;
    float aCoef = 0.f, rCoef = 0.f, env = 0.f, curGainDb = 0.f;
    int counter = 0;
};

} // namespace mono

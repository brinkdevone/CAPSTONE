#pragma once
// ============================================================================
//  CAPSTONE — Compresseur multibande 4 bandes (Linkwitz-Riley 4, phase recalée)
//
//  L'arbre de séparation est compensé par passe-tout croisés : la somme des
//  quatre bandes non traitées reproduit exactement l'entrée (à un passe-tout
//  global près). Sans cette compensation, un multibande « creuse » le spectre
//  autour de chaque fréquence de coupure, même tous réglages à zéro.
// ============================================================================
#include "Biquad.h"

namespace mono {

/** Compresseur compact en domaine dB, utilisé par bande. */
class BandCompressor
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; grDb = 0.0f; }

    void setParams (float thr, float ratio_, float attMs, float relMs, float kneeDb_, float makeupDb) noexcept
    {
        threshDb = thr; ratio = std::max (1.0f, ratio_); kneeDb = std::max (0.1f, kneeDb_);
        aCoef = timeCoef (attMs, sr); rCoef = timeCoef (relMs, sr);
        makeup = dbToGain (makeupDb);
    }

    /** Détection stéréo-liée : renvoie le gain linéaire à appliquer. */
    inline float computeGain (float detect) noexcept
    {
        const float inDb = gainToDb (detect);
        const float over = inDb - threshDb;
        float target;
        if (2.0f * over < -kneeDb)            target = 0.0f;
        else if (2.0f * std::abs (over) <= kneeDb)
        {
            const float t = over + kneeDb * 0.5f;
            target = (1.0f / ratio - 1.0f) * (t * t) / (2.0f * kneeDb);
        }
        else target = (1.0f / ratio - 1.0f) * over;

        const float c = (target < grDb) ? aCoef : rCoef;
        grDb = c * grDb + (1.0f - c) * target;
        return dbToGain (grDb) * makeup;
    }

    float getGrDb() const noexcept { return grDb; }
    void  resetGr()  noexcept { grDb = 0.0f; }

private:
    double sr = 48000.0;
    float threshDb = -20.f, ratio = 2.f, kneeDb = 6.f, makeup = 1.f;
    float aCoef = 0.f, rCoef = 0.f, grDb = 0.f;
};

// ---------------------------------------------------------------------------
class Multiband
{
public:
    static constexpr int kBands = 4;

    void prepare (double sampleRate, int numCh) noexcept
    {
        sr = sampleRate; nCh = std::min (numCh, 2);
        for (auto& c : comp) c.prepare (sr);
        reset();
    }

    void reset() noexcept
    {
        xLow.reset(); xMid.reset(); xHigh.reset();
        for (auto& a : apHi) a.reset();
        for (auto& a : apLo) a.reset();
        for (auto& c : comp) c.resetGr();
    }

    void setCrossovers (float fLow, float fMid, float fHigh) noexcept
    {
        // On garde l'ordre strict pour éviter des bandes croisées incohérentes.
        fLow  = clampf (fLow,  30.0f, 500.0f);
        fMid  = clampf (fMid,  std::max (fLow  * 1.2f, 200.0f),  4000.0f);
        fHigh = clampf (fHigh, std::max (fMid  * 1.2f, 1500.0f), 16000.0f);
        xMid.setCrossover  (sr, fMid);
        xLow.setCrossover  (sr, fLow);
        xHigh.setCrossover (sr, fHigh);
        // Une instance par bande : partager un filtre entre deux flux
        // reviendrait à les entrelacer et détruirait la reconstruction.
        for (auto& a : apHi) a.setFreq (sr, fHigh);
        for (auto& a : apLo) a.setFreq (sr, fLow);
    }

    void setBand (int b, float thr, float ratio, float att, float rel, float knee,
                  float makeupDb, float bandGainDb, bool bypass) noexcept
    {
        comp[b].setParams (thr, ratio, att, rel, knee, makeupDb);
        gain[b]   = dbToGain (bandGainDb);
        bypassed[b] = bypass;
    }

    void setSoloBand (int b) noexcept { solo = b; }   // -1 = pas de solo

    void process (float* const* io, int numCh, int n) noexcept
    {
        const int ch = std::min (numCh, nCh);
        for (int i = 0; i < n; ++i)
        {
            float band[kBands][2] {};
            float detect[kBands] { 0.f, 0.f, 0.f, 0.f };

            for (int c = 0; c < ch; ++c)
            {
                float lo2, hi2;
                xMid.process (c, io[c][i], lo2, hi2);          // séparation principale

                float b0, b1, b2, b3;
                xLow.process  (c, lo2, b0, b1);                // grave / bas-médium
                xHigh.process (c, hi2, b2, b3);                // haut-médium / aigu

                // Recalage de phase croisé : chaque moitié traverse le passe-tout
                // de l'autre crossover, la somme redevient plate.
                b0 = apHi[0].process (c, b0);
                b1 = apHi[1].process (c, b1);
                b2 = apLo[0].process (c, b2);
                b3 = apLo[1].process (c, b3);

                band[0][c] = b0; band[1][c] = b1; band[2][c] = b2; band[3][c] = b3;
                for (int k = 0; k < kBands; ++k) detect[k] = std::max (detect[k], std::abs (band[k][c]));
            }

            for (int k = 0; k < kBands; ++k)
            {
                const float g = bypassed[k] ? 1.0f : comp[k].computeGain (detect[k]);
                for (int c = 0; c < ch; ++c) band[k][c] *= g * gain[k];
            }

            for (int c = 0; c < ch; ++c)
            {
                float sum = 0.0f;
                for (int k = 0; k < kBands; ++k)
                    if (solo < 0 || solo == k) sum += band[k][c];
                io[c][i] = sum;
            }
        }
    }

    float getGrDb (int b) const noexcept { return comp[b].getGrDb(); }

private:
    double sr = 48000.0; int nCh = 2;
    LinkwitzRiley4 xLow, xMid, xHigh;
    AllpassLR4 apHi[2], apLo[2];
    BandCompressor comp[kBands];
    float gain[kBands] { 1.f, 1.f, 1.f, 1.f };
    bool  bypassed[kBands] { false, false, false, false };
    int   solo = -1;
};

} // namespace mono

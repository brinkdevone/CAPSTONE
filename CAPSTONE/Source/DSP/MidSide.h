#pragma once
// ============================================================================
//  CAPSTONE — Traitement Mid / Side
//
//  En mastering, l'axe M/S est aussi important que l'axe fréquentiel :
//  éclaircir les côtés ouvre le mix sans toucher à la voix au centre, et
//  recentrer le grave évite qu'un master s'effondre en mono.
// ============================================================================
#include "Biquad.h"

namespace mono {

class MidSideSection
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        xover.reset();
        for (int i = 0; i < 2; ++i) { midLo[i].makeBypass(); midHi[i].makeBypass();
                                      sideLo[i].makeBypass(); sideHi[i].makeBypass(); }
    }

    void setParams (float bassMonoHz, float width_,
                    float midLoF, float midLoG, float midHiF, float midHiG,
                    float sideLoF, float sideLoG, float sideHiF, float sideHiG) noexcept
    {
        width = clampf (width_, 0.0f, 2.0f);
        monoActive = bassMonoHz > 21.0f;
        if (monoActive) xover.setCrossover (sr, bassMonoHz);

        midLo[0].makeLowShelf   (sr, midLoF,  0.72, midLoG);
        midHi[0].makeHighShelf  (sr, midHiF,  0.72, midHiG);
        sideLo[0].makeLowShelf  (sr, sideLoF, 0.72, sideLoG);
        sideHi[0].makeHighShelf (sr, sideHiF, 0.72, sideHiG);
        // même jeu de coefficients pour la seconde instance (états séparés)
        midLo[1] = midLo[0];  midHi[1] = midHi[0];
        sideLo[1] = sideLo[0]; sideHi[1] = sideHi[0];
        midLo[1].reset(); midHi[1].reset(); sideLo[1].reset(); sideHi[1].reset();
    }

    void process (float* const* io, int numCh, int n) noexcept
    {
        if (numCh < 2) return;
        for (int i = 0; i < n; ++i)
        {
            float L = io[0][i], R = io[1][i];
            float monoLow = 0.0f;

            if (monoActive)
            {
                float lLo, lHi, rLo, rHi;
                xover.process (0, L, lLo, lHi);
                xover.process (1, R, rLo, rHi);
                monoLow = 0.5f * (lLo + rLo);   // le grave part au centre
                L = lHi; R = rHi;
            }

            float M = 0.5f * (L + R);
            float S = 0.5f * (L - R) * width;

            M = midHi[0].process  (midLo[0].process  (M));
            S = sideHi[0].process (sideLo[0].process (S));

            io[0][i] = monoLow + (M + S);
            io[1][i] = monoLow + (M - S);
        }
    }

private:
    double sr = 48000.0;
    LinkwitzRiley4 xover;
    Biquad midLo[2], midHi[2], sideLo[2], sideHi[2];
    float width = 1.0f;
    bool monoActive = false;
};

} // namespace mono

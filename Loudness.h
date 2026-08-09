#pragma once
// ============================================================================
//  CAPSTONE — Mesure de loudness conforme ITU-R BS.1770-4 / EBU R128
//
//  En mastering, la mesure n'est pas un accessoire : c'est elle qui décide.
//  Ce module fournit LUFS momentané / court terme / intégré (avec gating),
//  la plage de loudness (LRA, EBU Tech 3342), le true peak (suréchantillonné
//  ×4) et la corrélation de phase.
// ============================================================================
#include "Biquad.h"
#include <vector>
#include <array>
#include <cstring>

namespace mono {

// ---------------------------------------------------------------------------
//  Pondération K : passe-haut RLB + plateau aigu modélisant la tête
//  Coefficients dérivés analytiquement pour toute fréquence d'échantillonnage.
// ---------------------------------------------------------------------------
class KWeighting
{
public:
    void prepare (double sr) noexcept
    {
        constexpr double pi = 3.14159265358979323846;

        // Étage 1 — plateau aigu (+4 dB au-dessus de ~1,5 kHz)
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / sr);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.setDirect ((Vh + Vb * K / Q + K * K) / a0,
                             2.0 * (K * K - Vh) / a0,
                             (Vh - Vb * K / Q + K * K) / a0,
                             2.0 * (K * K - 1.0) / a0,
                             (1.0 - K / Q + K * K) / a0);
        }
        // Étage 2 — passe-haut RLB (-3 dB vers 60 Hz)
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (pi * f0 / sr);
            const double d = 1.0 + K / Q + K * K;
            hp.setDirect (1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / d, (1.0 - K / Q + K * K) / d);
        }
        reset();
    }
    void reset() noexcept { shelf.reset(); hp.reset(); }
    inline float process (float x) noexcept { return hp.process (shelf.process (x)); }
private:
    Biquad shelf, hp;
};

// ---------------------------------------------------------------------------
//  TRUE PEAK — interpolateur polyphase ×4 (ITU-R BS.1770-4 Annexe 2)
//  Un signal à 0 dBFS échantillon peut dépasser +3 dB entre deux échantillons.
//  Sans cette mesure, un master « à -0,1 dBFS » écrête après encodage MP3/AAC.
// ---------------------------------------------------------------------------
class TruePeakDetector
{
public:
    static constexpr int kTaps  = 24;   // par phase
    static constexpr int kPhase = 4;    // suréchantillonnage ×4

    TruePeakDetector() { buildKernel(); }

    void prepare (int numCh) noexcept
    {
        nCh = std::min (numCh, 2);
        for (size_t c = 0; c < 2; ++c) { hist[c].fill (0.0f); pos[c] = 0; }
    }

    /** Renvoie la crête inter-échantillon (linéaire) pour un échantillon donné. */
    inline float process (int ch, float x) noexcept
    {
        auto& h = hist[(size_t) ch];
        h[(size_t) pos[ch]] = x;
        pos[ch] = (pos[ch] + 1) % kTaps;

        float peak = std::abs (x);
        for (int p = 0; p < kPhase; ++p)
        {
            float acc = 0.0f;
            int idx = pos[ch];                       // plus ancien échantillon
            for (int k = 0; k < kTaps; ++k)
            {
                acc += h[(size_t) idx] * kernel[(size_t) (p * kTaps + (kTaps - 1 - k))];
                idx = (idx + 1) % kTaps;
            }
            peak = std::max (peak, std::abs (acc));
        }
        return peak;
    }

private:
    void buildKernel()
    {
        // Sinc fenêtré Blackman-Harris, coupure à la Nyquist d'origine.
        constexpr double pi = 3.14159265358979323846;
        const int  N = kTaps * kPhase;
        const double centre = (N - 1) * 0.5;
        std::array<double, (size_t) (kTaps * kPhase)> h {};
        double sum = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const double t = (i - centre) / (double) kPhase;
            const double s = (std::abs (t) < 1e-9) ? 1.0 : std::sin (pi * t) / (pi * t);
            // Fenêtre de Blackman : transition plus étroite que Blackman-Harris,
            // donc meilleure précision près de Nyquist — c'est justement là que
            // se cachent les crêtes inter-échantillon qu'on cherche à mesurer.
            const double r = (double) i / (double) (N - 1);
            const double w = 0.42 - 0.5 * std::cos (2 * pi * r) + 0.08 * std::cos (4 * pi * r);
            h[(size_t) i] = s * w;
            sum += h[(size_t) i];
        }
        // Normalisation : gain unité par phase (chaque phase somme à 1).
        for (int p = 0; p < kPhase; ++p)
        {
            double ps = 0.0;
            for (int k = 0; k < kTaps; ++k) ps += h[(size_t) (k * kPhase + p)];
            for (int k = 0; k < kTaps; ++k)
                kernel[(size_t) (p * kTaps + k)] = (float) (h[(size_t) (k * kPhase + p)] / (std::abs (ps) > 1e-12 ? ps : 1.0));
        }
        (void) sum;
    }

    std::array<float, (size_t) (kTaps * kPhase)> kernel {};
    std::array<std::array<float, kTaps>, 2> hist {};
    int pos[2] { 0, 0 };
    int nCh = 2;
};

// ---------------------------------------------------------------------------
//  MESUREUR DE LOUDNESS COMPLET
// ---------------------------------------------------------------------------
class LoudnessMeter
{
public:
    static constexpr int    kBins     = 1000;
    static constexpr double kBinMin   = -70.0;
    static constexpr double kBinWidth = 0.1;

    void prepare (double sampleRate, int numCh)
    {
        sr = sampleRate; nCh = std::min (numCh, 2);
        for (int c = 0; c < 2; ++c) { kw[c].prepare (sr); }
        tp.prepare (nCh);

        subBlockLen = (int) std::round (sr * 0.1);          // pas de 100 ms
        subSum.assign (30, 0.0);                            // 3 s d'historique
        reset();
    }

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c) kw[c].reset();
        std::fill (subSum.begin(), subSum.end(), 0.0);
        subWrite = 0; subCount = 0; accum = 0.0; accumN = 0;
        histI.fill (0); histS.fill (0);
        truePeak = 0.0f; samplePeak = 0.0f;
        corrLL = corrRR = corrLR = 1.0e-12;
        momentary = shortTerm = -200.0f;
    }

    void processBlock (const float* const* in, int numChIn, int n) noexcept
    {
        const int ch = std::min (numChIn, nCh);
        for (int i = 0; i < n; ++i)
        {
            double sumSq = 0.0;
            float l = 0.0f, r = 0.0f;
            for (int c = 0; c < ch; ++c)
            {
                const float x = in[c][i];
                samplePeak = std::max (samplePeak, std::abs (x));
                truePeak   = std::max (truePeak, tp.process (c, x));
                const float k = kw[c].process (x);
                sumSq += (double) k * k;                    // pondération G = 1.0 (L/R)
                if (c == 0) l = x; else r = x;
            }
            if (ch == 1) r = l;

            // corrélation glissante (~300 ms)
            corrLL = corrDecay * corrLL + (double) l * l;
            corrRR = corrDecay * corrRR + (double) r * r;
            corrLR = corrDecay * corrLR + (double) l * r;

            accum += sumSq; ++accumN;
            if (accumN >= subBlockLen) { pushSubBlock (accum / (double) accumN); accum = 0.0; accumN = 0; }
        }
    }

    // --- lectures ----------------------------------------------------------
    float getMomentary()  const noexcept { return momentary; }
    float getShortTerm()  const noexcept { return shortTerm; }
    float getIntegrated() const noexcept { return gatedLoudness (histI, 10.0); }
    float getTruePeakDb() const noexcept { return gainToDb (truePeak); }
    float getSamplePeakDb() const noexcept { return gainToDb (samplePeak); }
    void  resetPeaks() noexcept { truePeak = 0.0f; samplePeak = 0.0f; }

    float getCorrelation() const noexcept
    {
        const double d = std::sqrt (corrLL * corrRR);
        return d > 1.0e-12 ? (float) clampf ((float) (corrLR / d), -1.0f, 1.0f) : 1.0f;
    }

    /** Plage de loudness (EBU Tech 3342) : 95e centile - 10e centile. */
    float getLRA() const noexcept
    {
        const float thr = gatedThreshold (histS, 20.0);
        if (thr < -190.0f) return 0.0f;
        long long total = 0;
        for (int i = 0; i < kBins; ++i) if (binLoudness (i) >= thr) total += histS[(size_t) i];
        if (total < 2) return 0.0f;

        auto centile = [&] (double frac)
        {
            const long long want = (long long) (frac * (double) total);
            long long seen = 0;
            for (int i = 0; i < kBins; ++i)
            {
                if (binLoudness (i) < thr) continue;
                seen += histS[(size_t) i];
                if (seen >= want) return (float) binLoudness (i);
            }
            return (float) binLoudness (kBins - 1);
        };
        return std::max (0.0f, centile (0.95) - centile (0.10));
    }

private:
    void pushSubBlock (double meanSq) noexcept
    {
        subSum[(size_t) subWrite] = meanSq;
        subWrite = (subWrite + 1) % (int) subSum.size();
        if (subCount < (int) subSum.size()) ++subCount;

        auto windowMean = [&] (int blocks) -> double
        {
            if (subCount < blocks) return -1.0;
            double s = 0.0;
            for (int k = 1; k <= blocks; ++k)
                s += subSum[(size_t) ((subWrite - k + (int) subSum.size()) % (int) subSum.size())];
            return s / blocks;
        };

        const double m400 = windowMean (4);    // 400 ms — momentané
        if (m400 > 0.0)
        {
            momentary = (float) (-0.691 + 10.0 * std::log10 (m400));
            addToHist (histI, momentary);
        }
        const double m3s = windowMean (30);    // 3 s — court terme
        if (m3s > 0.0)
        {
            shortTerm = (float) (-0.691 + 10.0 * std::log10 (m3s));
            addToHist (histS, shortTerm);
        }
    }

    static double binLoudness (int i) noexcept { return kBinMin + (i + 0.5) * kBinWidth; }
    static double binEnergy   (int i) noexcept { return std::pow (10.0, (binLoudness (i) + 0.691) / 10.0); }

    static void addToHist (std::array<long long, kBins>& h, float loudness) noexcept
    {
        if (loudness < kBinMin) return;                       // porte absolue -70 LUFS
        const int i = (int) ((loudness - kBinMin) / kBinWidth);
        if (i >= 0 && i < kBins) ++h[(size_t) i];
    }

    /** Seuil relatif = moyenne énergétique des blocs retenus - offset (10 ou 20 LU). */
    static float gatedThreshold (const std::array<long long, kBins>& h, double offset) noexcept
    {
        double e = 0.0; long long n = 0;
        for (int i = 0; i < kBins; ++i) if (h[(size_t) i]) { e += binEnergy (i) * (double) h[(size_t) i]; n += h[(size_t) i]; }
        if (n == 0) return -200.0f;
        return (float) (-0.691 + 10.0 * std::log10 (e / (double) n) - offset);
    }

    static float gatedLoudness (const std::array<long long, kBins>& h, double offset) noexcept
    {
        const float thr = gatedThreshold (h, offset);
        if (thr < -190.0f) return -200.0f;
        double e = 0.0; long long n = 0;
        for (int i = 0; i < kBins; ++i)
            if (h[(size_t) i] && binLoudness (i) >= thr) { e += binEnergy (i) * (double) h[(size_t) i]; n += h[(size_t) i]; }
        if (n == 0) return -200.0f;
        return (float) (-0.691 + 10.0 * std::log10 (e / (double) n));
    }

    double sr = 48000.0; int nCh = 2;
    KWeighting kw[2];
    TruePeakDetector tp;

    int subBlockLen = 4800, subWrite = 0, subCount = 0, accumN = 0;
    double accum = 0.0;
    std::vector<double> subSum;

    std::array<long long, kBins> histI {}, histS {};
    float momentary = -200.f, shortTerm = -200.f;
    float truePeak = 0.f, samplePeak = 0.f;
    double corrLL = 1e-12, corrRR = 1e-12, corrLR = 1e-12;
    static constexpr double corrDecay = 0.99993;   // ~300 ms à 48 kHz
};

} // namespace mono

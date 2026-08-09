#pragma once
// ============================================================================
//  CAPSTONE — Saturation et écrêtage de mastering (suréchantillonnés)
//
//  Le clipper placé AVANT le limiteur est le secret du niveau moderne :
//  1 à 2 dB d'écrêtage doux sur les crêtes de caisse claire libèrent autant
//  de marge, sans le pompage qu'aurait produit le limiteur pour le même gain.
// ============================================================================
#include "Oversampler.h"
#include <vector>

namespace mono {

enum class ColourMode { Tape = 0, Transformer = 1, Tube = 2 };
enum class ClipMode   { Soft = 0, Medium = 1, Hard = 2 };

class ColourStage
{
public:
    void prepare (double sampleRate, int numCh, int maxBlock)
    {
        sr = sampleRate; nCh = std::min (numCh, 2);
        os.prepare (sr, maxBlock, 1);            // ×2 suffit pour une saturation douce
        work.assign ((size_t) maxBlock * 2 + 8, 0.0f);
        dry.assign  ((size_t) maxBlock, 0.0f);
        for (int c = 0; c < 2; ++c) { dc[c].makeHighpass (sr, 15.0); dc[c].reset(); }
    }

    void setParams (ColourMode m, float driveDb, float mix_) noexcept
    {
        mode = m; drive = dbToGain (driveDb); mix = clampf (mix_, 0.0f, 1.0f);
        comp = 1.0f / (1.0f + 0.5f * (drive - 1.0f));
    }

    void process (float* const* io, int numCh, int n) noexcept
    {
        if (mix < 0.001f) return;
        const int ch = std::min (numCh, nCh);
        for (int c = 0; c < ch; ++c)
        {
            std::copy (io[c], io[c] + n, dry.begin());
            float* up = os.upsample (c, io[c], n);
            std::copy (up, up + 2 * n, work.begin());
            for (int i = 0; i < 2 * n; ++i) work[(size_t) i] = shape (work[(size_t) i] * drive) * comp;
            os.downsample (c, work.data(), io[c], n);
            for (int i = 0; i < n; ++i)
                io[c][i] = dry[(size_t) i] * (1.0f - mix) + dc[c].process (io[c][i]) * mix;
        }
    }

private:
    inline float shape (float x) const noexcept
    {
        switch (mode)
        {
            case ColourMode::Tape:        return std::tanh (x);
            case ColourMode::Transformer: return x / (1.0f + std::abs (x) * 0.72f);
            case ColourMode::Tube:
            default:
            {
                const float b = 0.12f;                       // asymétrie => harmonique 2
                return std::tanh (x + b) - std::tanh (b);
            }
        }
    }

    double sr = 48000.0; int nCh = 2;
    ColourMode mode = ColourMode::Tape;
    Oversampler os;
    std::vector<float> work, dry;
    OnePole dc[2];
    float drive = 1.f, mix = 0.f, comp = 1.f;
};

// ---------------------------------------------------------------------------
class MasterClipper
{
public:
    void prepare (double sampleRate, int numCh, int maxBlock)
    {
        sr = sampleRate; nCh = std::min (numCh, 2); block = maxBlock;
        os.prepare (sr, maxBlock, 2);            // ×4 : indispensable pour l'ecretage
        work.assign ((size_t) maxBlock * 4 + 8, 0.0f);
    }

    void setParams (bool on, ClipMode m, float driveDb, float ceilingDb, bool oversample_) noexcept
    {
        enabled = on; mode = m;
        drive = dbToGain (driveDb);
        ceiling = dbToGain (ceilingDb);
        oversample = oversample_;
    }

    void process (float* const* io, int numCh, int n) noexcept
    {
        if (! enabled) return;
        const int ch = std::min (numCh, nCh);
        for (int c = 0; c < ch; ++c)
        {
            if (oversample)
            {
                const int F = os.factor();
                float* up = os.upsample (c, io[c], n);
                std::copy (up, up + F * n, work.begin());
                for (int i = 0; i < F * n; ++i) work[(size_t) i] = clip (work[(size_t) i] * drive);
                os.downsample (c, work.data(), io[c], n);
                // Pas de clamp après décimation : il ré-introduirait exactement
                // le repliement que le suréchantillonnage vient d'éviter.
            }
            else
                for (int i = 0; i < n; ++i) io[c][i] = clip (io[c][i] * drive);
        }
    }

private:
    inline float clip (float x) const noexcept
    {
        const float t = x / ceiling;
        float y;
        switch (mode)
        {
            case ClipMode::Soft:   y = std::tanh (t); break;
            case ClipMode::Medium: y = t / std::pow (1.0f + std::pow (std::abs (t), 4.0f), 0.25f); break;
            case ClipMode::Hard:
            default:               y = clampf (t, -1.0f, 1.0f); break;
        }
        return y * ceiling;
    }

    double sr = 48000.0; int nCh = 2;
    ClipMode mode = ClipMode::Soft;
    Oversampler os;
    std::vector<float> work;
    int block = 512;
    float drive = 1.f, ceiling = 1.f;
    bool enabled = false, oversample = true;
};

} // namespace mono

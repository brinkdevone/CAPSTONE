#pragma once
// ============================================================================
//  MONOLITH — Ballistics & maths de base (C++ pur, aucune dépendance JUCE)
// ============================================================================
#include <cmath>
#include <algorithm>

namespace mono {

constexpr float kMinGain = 1.0e-9f;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  noexcept { return 20.0f * std::log10 (std::max (std::abs (g), kMinGain)); }
inline float clampf   (float v, float lo, float hi) noexcept { return std::min (std::max (v, lo), hi); }

/** Coefficient d'un lissage exponentiel one-pole pour un temps en ms. */
inline float timeCoef (float ms, double sr) noexcept
{
    ms = std::max (0.001f, ms);
    return std::exp (-1.0f / (0.001f * ms * (float) sr));
}

/** Suiveur d'enveloppe classique attaque/relâche (domaine linéaire). */
class EnvelopeFollower
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; setTimes (attMs, relMs); reset(); }
    void reset() noexcept { env = 0.0f; }

    void setTimes (float attackMs, float releaseMs) noexcept
    {
        attMs = attackMs; relMs = releaseMs;
        aCoef = timeCoef (attMs, sr);
        rCoef = timeCoef (relMs, sr);
    }

    inline float process (float rectified) noexcept
    {
        const float c = (rectified > env) ? aCoef : rCoef;
        env = c * env + (1.0f - c) * rectified;
        return env;
    }

    float value() const noexcept { return env; }

private:
    double sr = 48000.0;
    float attMs = 10.0f, relMs = 100.0f, aCoef = 0.0f, rCoef = 0.0f, env = 0.0f;
};

/** Lissage de paramètre (anti-zipper), rampe linéaire. */
class SmoothedValue
{
public:
    void prepare (double sr, float rampMs = 20.0f) noexcept
    {
        steps = std::max (1, (int) (0.001 * rampMs * sr));
        current = target; counter = 0; inc = 0.0f;
    }
    void setTargetValue (float v) noexcept
    {
        if (std::abs (v - target) < 1.0e-9f) return;
        target = v; inc = (target - current) / (float) steps; counter = steps;
    }
    void snapTo (float v) noexcept { target = current = v; counter = 0; inc = 0.0f; }
    inline float next() noexcept
    {
        if (counter > 0) { current += inc; --counter; if (counter == 0) current = target; }
        return current;
    }
    float value() const noexcept { return current; }
private:
    float target = 0.0f, current = 0.0f, inc = 0.0f;
    int steps = 480, counter = 0;
};

} // namespace mono

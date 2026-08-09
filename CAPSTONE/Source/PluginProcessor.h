#pragma once
// ============================================================================
//  CAPSTONE — Mastering Suite
// ============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"
#include "DSP/Loudness.h"
#include "DSP/Multiband.h"
#include "DSP/DynamicEq.h"
#include "DSP/MidSide.h"
#include "DSP/Limiter.h"
#include "DSP/Dither.h"
#include "DSP/Saturation.h"
#include <atomic>

class CapstoneProcessor : public juce::AudioProcessor
{
public:
    CapstoneProcessor();
    ~CapstoneProcessor() override = default;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CAPSTONE"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) mono::factoryPresets().size() + 1; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** Remet à zéro l'intégration LUFS et les crêtes — à faire avant chaque lecture. */
    void resetMeters() { resetRequested.store (true); }

    juce::AudioProcessorValueTreeState apvts;

    struct Meters
    {
        std::atomic<float> lufsM { -200.f }, lufsS { -200.f }, lufsI { -200.f };
        std::atomic<float> lra { 0.f }, truePeak { -200.f }, samplePeak { -200.f };
        std::atomic<float> correlation { 1.f };
        std::atomic<float> inLufsI { -200.f }, inTruePeak { -200.f };
        std::atomic<float> limGr { 0.f }, mbGr[4] { {0.f},{0.f},{0.f},{0.f} };
        std::atomic<float> dyn1 { 0.f }, dyn2 { 0.f };
    } meters;

private:
    void pullParameters();
    float p (const char* id) const noexcept { return apvts.getRawParameterValue (id)->load(); }

    double sr = 48000.0;
    int maxBlock = 512, numCh = 2, currentProgram = 0, reportedLatency = 0;
    std::atomic<bool> resetRequested { false };

    mono::Biquad          rumble[2];
    mono::Biquad          corrQ[2][2];
    mono::DynamicBand     dyn1, dyn2;
    mono::Multiband       mb;
    mono::Biquad          tone[2][4];
    mono::MidSideSection  ms;
    mono::ColourStage     colour;
    mono::MasterClipper   clipper;
    mono::LookaheadLimiter limiter;
    mono::Dither          dither;
    mono::LoudnessMeter   meterIn, meterOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CapstoneProcessor)
};

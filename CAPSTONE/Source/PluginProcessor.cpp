#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mono;

CapstoneProcessor::CapstoneProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CAPSTONE", createLayout())
{}

bool CapstoneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out;
}

void CapstoneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; maxBlock = samplesPerBlock;
    numCh = juce::jmax (1, getTotalNumOutputChannels());

    for (int c = 0; c < 2; ++c)
    {
        rumble[c].reset();
        for (int i = 0; i < 2; ++i) corrQ[c][i].reset();
        for (int i = 0; i < 4; ++i) tone[c][i].reset();
    }
    dyn1.prepare (sr, numCh); dyn2.prepare (sr, numCh);
    mb.prepare (sr, numCh);
    ms.prepare (sr);
    colour.prepare  (sr, numCh, samplesPerBlock);
    clipper.prepare (sr, numCh, samplesPerBlock);
    limiter.prepare (sr, numCh, p (pid::limLook));
    dither.prepare (numCh);
    meterIn.prepare (sr, numCh);
    meterOut.prepare (sr, numCh);

    reportedLatency = limiter.getLatencySamples();
    setLatencySamples (reportedLatency);
}

void CapstoneProcessor::pullParameters()
{
    for (int c = 0; c < 2; ++c)
    {
        if (p (pid::rumbleOn) > 0.5f) rumble[c].makeHighpass (sr, p (pid::rumbleF), 0.707);
        else                          rumble[c].makeBypass();

        if (p (pid::cqOn) > 0.5f)
        {
            corrQ[c][0].makePeak (sr, p (pid::cq1F), p (pid::cq1Q), p (pid::cq1G));
            corrQ[c][1].makePeak (sr, p (pid::cq2F), p (pid::cq2Q), p (pid::cq2G));
        }
        else { corrQ[c][0].makeBypass(); corrQ[c][1].makeBypass(); }

        if (p (pid::teOn) > 0.5f)
        {
            tone[c][0].makeLowShelf  (sr, p (pid::teLoF), 0.72, p (pid::teLoG));
            tone[c][1].makePeak      (sr, p (pid::teLmF), p (pid::teLmQ), p (pid::teLmG));
            tone[c][2].makePeak      (sr, p (pid::teHmF), p (pid::teHmQ), p (pid::teHmG));
            tone[c][3].makeHighShelf (sr, p (pid::teHiF), 0.72, p (pid::teHiG));
        }
        else for (int i = 0; i < 4; ++i) tone[c][i].makeBypass();
    }

    dyn1.setParams (p (pid::dq1On) > 0.5f, p (pid::dq1F), p (pid::dq1Q),
                    p (pid::dq1Thr), p (pid::dq1R), 8.0f, 90.0f, p (pid::dq1Rng));
    dyn2.setParams (p (pid::dq2On) > 0.5f, p (pid::dq2F), p (pid::dq2Q),
                    p (pid::dq2Thr), p (pid::dq2R), 5.0f, 70.0f, p (pid::dq2Rng));

    mb.setCrossovers (p (pid::mbX1), p (pid::mbX2), p (pid::mbX3));
    const char* thrIds[] = { pid::mb1Thr, pid::mb2Thr, pid::mb3Thr, pid::mb4Thr };
    const char* ratIds[] = { pid::mb1R,   pid::mb2R,   pid::mb3R,   pid::mb4R   };
    const char* gainIds[]= { pid::mb1G,   pid::mb2G,   pid::mb3G,   pid::mb4G   };
    for (int b = 0; b < 4; ++b)
        mb.setBand (b, p (thrIds[b]), p (ratIds[b]), p (pid::mbAtt), p (pid::mbRel),
                    6.0f, 0.0f, p (gainIds[b]), false);
    mb.setSoloBand ((int) p (pid::mbSolo) - 1);

    ms.setParams (p (pid::msOn) > 0.5f ? p (pid::msBassMono) : 0.0f,
                  p (pid::msOn) > 0.5f ? p (pid::msWidth) : 1.0f,
                  p (pid::msMidLoF), p (pid::msOn) > 0.5f ? p (pid::msMidLoG) : 0.f,
                  p (pid::msMidHiF), p (pid::msOn) > 0.5f ? p (pid::msMidHiG) : 0.f,
                  p (pid::msSidLoF), p (pid::msOn) > 0.5f ? p (pid::msSidLoG) : 0.f,
                  p (pid::msSidHiF), p (pid::msOn) > 0.5f ? p (pid::msSidHiG) : 0.f);

    colour.setParams ((ColourMode) (int) p (pid::colMode), p (pid::colDrive),
                      p (pid::colOn) > 0.5f ? p (pid::colMix) : 0.0f);

    clipper.setParams (p (pid::clOn) > 0.5f, (ClipMode) (int) p (pid::clMode),
                       p (pid::clDrive), dbToGain (p (pid::limCeil)), p (pid::clOs) > 0.5f);

    limiter.setParams (p (pid::limCeil), p (pid::limRel),
                       p (pid::limOn) > 0.5f ? p (pid::limGain) : 0.0f);

    static const int bitTable[] = { 16, 20, 24 };
    dither.setParams ((DitherMode) (int) p (pid::ditMode), bitTable[(int) p (pid::ditBits)]);
}

void CapstoneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (buffer.getNumChannels(), 2);

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    if (resetRequested.exchange (false)) { meterIn.reset(); meterOut.reset(); }

    // La latence du limiteur peut changer si l'utilisateur bouge le lookahead.
    if (limiter.getLatencySamples() != reportedLatency)
    {
        limiter.prepare (sr, numCh, p (pid::limLook));
        reportedLatency = limiter.getLatencySamples();
        setLatencySamples (reportedLatency);
    }

    float* io[2] = { buffer.getWritePointer (0), ch > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0) };
    const float* cio[2] = { io[0], io[1] };

    meterIn.processBlock (cio, ch, n);
    meters.inLufsI.store    (meterIn.getIntegrated());
    meters.inTruePeak.store (meterIn.getTruePeakDb());

    if (p (pid::bypass) > 0.5f) return;
    pullParameters();

    // 1 — entrée
    { const float g = dbToGain (p (pid::inTrim));
      for (int c = 0; c < ch; ++c) juce::FloatVectorOperations::multiply (io[c], g, n); }

    // 2 — rumble + 3 — EQ correctif
    for (int c = 0; c < ch; ++c)
        for (int i = 0; i < n; ++i)
            io[c][i] = corrQ[c][1].process (corrQ[c][0].process (rumble[c].process (io[c][i])));

    // 4 — EQ dynamique
    dyn1.process (io, ch, n);  meters.dyn1.store (dyn1.getGainDb());
    dyn2.process (io, ch, n);  meters.dyn2.store (dyn2.getGainDb());

    // 5 — multibande
    if (p (pid::mbOn) > 0.5f || (int) p (pid::mbSolo) > 0)
    {
        mb.process (io, ch, n);
        for (int b = 0; b < 4; ++b) meters.mbGr[b].store (mb.getGrDb (b));
    }
    else for (int b = 0; b < 4; ++b) meters.mbGr[b].store (0.f);

    // 6 — EQ tonal
    for (int c = 0; c < ch; ++c)
        for (int i = 0; i < n; ++i)
        {
            float x = io[c][i];
            for (int k = 0; k < 4; ++k) x = tone[c][k].process (x);
            io[c][i] = x;
        }

    // 7 — Mid / Side
    if (ch > 1) ms.process (io, ch, n);

    // 8 — couleur, 9 — clipper, 10 — limiteur
    colour.process  (io, ch, n);
    clipper.process (io, ch, n);
    limiter.process (io, ch, n);
    meters.limGr.store (limiter.getGrDb());

    // 11 — sortie
    { const float g = dbToGain (p (pid::outTrim));
      for (int c = 0; c < ch; ++c) juce::FloatVectorOperations::multiply (io[c], g, n); }

    // 12 — dither, toujours en dernier
    dither.process (io, ch, n);

    for (int c = 0; c < ch; ++c)
        for (int i = 0; i < n; ++i) if (! std::isfinite (io[c][i])) io[c][i] = 0.0f;

    meterOut.processBlock (cio, ch, n);
    meters.lufsM.store      (meterOut.getMomentary());
    meters.lufsS.store      (meterOut.getShortTerm());
    meters.lufsI.store      (meterOut.getIntegrated());
    meters.lra.store        (meterOut.getLRA());
    meters.truePeak.store   (meterOut.getTruePeakDb());
    meters.samplePeak.store (meterOut.getSamplePeakDb());
    meters.correlation.store(meterOut.getCorrelation());
}

const juce::String CapstoneProcessor::getProgramName (int index)
{
    if (index <= 0) return "Init";
    const auto& P = factoryPresets();
    const int i = index - 1;
    if (i >= (int) P.size()) return "Init";
    return juce::String (P[(size_t) i].category) + " — " + P[(size_t) i].name;
}

void CapstoneProcessor::setCurrentProgram (int index)
{
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, index);
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    if (currentProgram == 0) return;
    for (const auto& v : factoryPresets()[(size_t) (currentProgram - 1)].values)
        if (auto* rp = apvts.getParameter (v.id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v.v));
}

void CapstoneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void CapstoneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* CapstoneProcessor::createEditor() { return new CapstoneEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CapstoneProcessor(); }

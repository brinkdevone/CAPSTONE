#include "PluginEditor.h"

using namespace mono;

static std::vector<juce::Rectangle<int>> split (juce::Rectangle<int> r, std::vector<float> w, int gap = 10)
{
    float total = 0.f; for (auto x : w) total += x;
    const int usable = r.getWidth() - gap * ((int) w.size() - 1);
    std::vector<juce::Rectangle<int>> out;
    for (size_t i = 0; i < w.size(); ++i)
    {
        const int width = (i + 1 == w.size()) ? r.getWidth() : (int) (usable * (w[i] / total));
        out.push_back (r.removeFromLeft (width));
        if (i + 1 < w.size()) r.removeFromLeft (gap);
    }
    return out;
}

CapstoneEditor::CapstoneEditor (CapstoneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    titleLabel.setText ("CAPSTONE", juce::dontSendNotification);
    titleLabel.setFont (makeFont (24.0f, true));
    titleLabel.setColour (juce::Label::textColourId, col::text);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("MASTERING SUITE — ITU-R BS.1770-4 / EBU R128", juce::dontSendNotification);
    subtitleLabel.setFont (makeFont (9.0f));
    subtitleLabel.setColour (juce::Label::textColourId, col::accent);
    addAndMakeVisible (subtitleLabel);

    juce::String lastCat;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        if (i == 0) { presetBox.addItem ("Init (neutre)", 1); continue; }
        const auto& pr = factoryPresets()[(size_t) i - 1];
        if (juce::String (pr.category) != lastCat) { presetBox.addSectionHeading (pr.category); lastCat = pr.category; }
        presetBox.addItem (pr.name, i + 1);
    }
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx < 0) return;
        proc.setCurrentProgram (idx);
        hintLabel.setText (idx > 0 ? factoryPresets()[(size_t) idx - 1].note : "", juce::dontSendNotification);
    };
    addAndMakeVisible (presetBox);

    bypassBtn.setClickingTogglesState (true);
    addAndMakeVisible (bypassBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pid::bypass, bypassBtn);

    resetBtn.onClick = [this] { proc.resetMeters(); };
    addAndMakeVisible (resetBtn);

    hintLabel.setFont (makeFont (10.0f));
    hintLabel.setColour (juce::Label::textColourId, col::textDim);
    addAndMakeVisible (hintLabel);

    buildSections();
    for (auto* c : { &rLufsI, &rLufsS, &rLufsM, &rTp, &rLra, &rPlr }) addAndMakeVisible (*c);
    addAndMakeVisible (corr);
    for (auto& b : barMb) addAndMakeVisible (b);
    addAndMakeVisible (barLim);

    setResizable (true, true);
    setResizeLimits (1020, 700, 1800, 1200);
    setSize (1180, 820);
    startTimerHz (20);
}

CapstoneEditor::~CapstoneEditor() { setLookAndFeel (nullptr); }

void CapstoneEditor::buildSections()
{
    auto& s = proc.apvts;
    auto mk = [&] (const juce::String& t, const juce::String& id = {}) -> Section*
    { auto* sec = new Section (t, id.isEmpty() ? nullptr : &s, id); sections.add (sec); addAndMakeVisible (sec); return sec; };

    secIn = mk ("Entree / cible");
    secIn->add (new Selector (s, pid::target,   "Cible"));
    secIn->add (new Knob     (s, pid::inTrim,   "Trim"));
    secIn->add (new Toggle   (s, pid::rumbleOn, "RUMBLE"));
    secIn->add (new Knob     (s, pid::rumbleF,  "Rumble Hz"));
    secIn->cellW = 78;

    secCorr = mk ("EQ correctif", pid::cqOn);
    secCorr->add (new Knob (s, pid::cq1F, "C1 Hz")); secCorr->add (new Knob (s, pid::cq1G, "C1 gain"));
    secCorr->add (new Knob (s, pid::cq1Q, "C1 Q"));
    secCorr->add (new Knob (s, pid::cq2F, "C2 Hz")); secCorr->add (new Knob (s, pid::cq2G, "C2 gain"));
    secCorr->add (new Knob (s, pid::cq2Q, "C2 Q"));

    secDyn = mk ("EQ dynamique");
    secDyn->add (new Toggle (s, pid::dq1On, "D1"));
    secDyn->add (new Knob (s, pid::dq1F, "Hz"));   secDyn->add (new Knob (s, pid::dq1Q, "Q"));
    secDyn->add (new Knob (s, pid::dq1Thr, "Seuil")); secDyn->add (new Knob (s, pid::dq1R, "Ratio"));
    secDyn->add (new Knob (s, pid::dq1Rng, "Plage"));
    secDyn->add (new Toggle (s, pid::dq2On, "D2"));
    secDyn->add (new Knob (s, pid::dq2F, "Hz"));   secDyn->add (new Knob (s, pid::dq2Q, "Q"));
    secDyn->add (new Knob (s, pid::dq2Thr, "Seuil")); secDyn->add (new Knob (s, pid::dq2R, "Ratio"));
    secDyn->add (new Knob (s, pid::dq2Rng, "Plage"));

    secMb = mk ("Multibande", pid::mbOn);
    secMb->add (new Knob (s, pid::mbX1, "Coupure 1")); secMb->add (new Knob (s, pid::mbX2, "Coupure 2"));
    secMb->add (new Knob (s, pid::mbX3, "Coupure 3")); secMb->add (new Knob (s, pid::mbAtt, "Attaque"));
    secMb->add (new Knob (s, pid::mbRel, "Relache"));  secMb->add (new Selector (s, pid::mbSolo, "Solo"));
    const char* T[] = { pid::mb1Thr, pid::mb2Thr, pid::mb3Thr, pid::mb4Thr };
    const char* R[] = { pid::mb1R,   pid::mb2R,   pid::mb3R,   pid::mb4R   };
    const char* G[] = { pid::mb1G,   pid::mb2G,   pid::mb3G,   pid::mb4G   };
    const char* N[] = { "Grave", "Bas-med", "Haut-med", "Aigu" };
    for (int b = 0; b < 4; ++b)
    {
        secMb->add (new Knob (s, T[b], juce::String (N[b]) + " s."));
        secMb->add (new Knob (s, R[b], "Ratio"));
        secMb->add (new Knob (s, G[b], "Gain"));
    }

    secTone = mk ("EQ tonal", pid::teOn);
    secTone->add (new Knob (s, pid::teLoF, "Assise Hz")); secTone->add (new Knob (s, pid::teLoG, "Assise"));
    secTone->add (new Knob (s, pid::teLmF, "Bas-med Hz"));secTone->add (new Knob (s, pid::teLmG, "Bas-med"));
    secTone->add (new Knob (s, pid::teLmQ, "Q"));
    secTone->add (new Knob (s, pid::teHmF, "Presence Hz"));secTone->add (new Knob (s, pid::teHmG, "Presence"));
    secTone->add (new Knob (s, pid::teHmQ, "Q"));
    secTone->add (new Knob (s, pid::teHiF, "Air Hz"));    secTone->add (new Knob (s, pid::teHiG, "Air"));

    secMs = mk ("Mid / Side", pid::msOn);
    secMs->add (new Knob (s, pid::msBassMono, "Bass mono")); secMs->add (new Knob (s, pid::msWidth, "Largeur"));
    secMs->add (new Knob (s, pid::msMidLoF, "M grave Hz")); secMs->add (new Knob (s, pid::msMidLoG, "M grave"));
    secMs->add (new Knob (s, pid::msMidHiF, "M aigu Hz"));  secMs->add (new Knob (s, pid::msMidHiG, "M aigu"));
    secMs->add (new Knob (s, pid::msSidLoF, "S grave Hz")); secMs->add (new Knob (s, pid::msSidLoG, "S grave"));
    secMs->add (new Knob (s, pid::msSidHiF, "S aigu Hz"));  secMs->add (new Knob (s, pid::msSidHiG, "S aigu"));

    secOut = mk ("Couleur / clipper / limiteur / sortie");
    secOut->add (new Toggle   (s, pid::colOn,   "COULEUR"));
    secOut->add (new Selector (s, pid::colMode, "Modele"));
    secOut->add (new Knob     (s, pid::colDrive,"Drive"));
    secOut->add (new Knob     (s, pid::colMix,  "Dosage"));
    secOut->add (new Toggle   (s, pid::clOn,    "CLIP"));
    secOut->add (new Selector (s, pid::clMode,  "Courbe"));
    secOut->add (new Knob     (s, pid::clDrive, "Clip drive"));
    secOut->add (new Toggle   (s, pid::clOs,    "OS x4"));
    secOut->add (new Toggle   (s, pid::limOn,   "LIMIT"));
    secOut->add (new Knob     (s, pid::limGain, "Gain"));
    secOut->add (new Knob     (s, pid::limCeil, "Plafond"));
    secOut->add (new Knob     (s, pid::limRel,  "Relache"));
    secOut->add (new Knob     (s, pid::limLook, "Lookahead"));
    secOut->add (new Knob     (s, pid::outTrim, "Sortie"));
    secOut->add (new Selector (s, pid::ditMode, "Dither"));
    secOut->add (new Selector (s, pid::ditBits, "Bits"));
    secOut->cellW = 72;

    secMeters = mk ("Telemetrie");
}

void CapstoneEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto head = getLocalBounds().removeFromTop (62).toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (head.reduced (14.0f, 8.0f), 5.0f);
    g.setColour (col::accent.withAlpha (0.45f));
    g.fillRect (14.0f, head.getBottom() - 8.0f, (float) getWidth() - 28.0f, 1.0f);
}

void CapstoneEditor::resized()
{
    auto r = getLocalBounds().reduced (14, 0);
    auto head = r.removeFromTop (62).reduced (0, 8);
    titleLabel.setBounds    (head.removeFromLeft (135).reduced (10, 0));
    subtitleLabel.setBounds (head.removeFromLeft (230).withTrimmedTop (14));
    bypassBtn.setBounds     (head.removeFromRight (80).reduced (2, 8));
    resetBtn.setBounds      (head.removeFromRight (105).reduced (2, 8));
    presetBox.setBounds     (head.removeFromLeft (juce::jmin (280, head.getWidth() - 180)).reduced (4, 9));
    hintLabel.setBounds     (head.reduced (10, 0));

    r.removeFromTop (6);
    const int gap = 10;

    { auto row = r.removeFromTop (150);
      auto c = split (row, { 0.80f, 1.05f, 1.60f });
      secIn->setBounds (c[0]); secCorr->setBounds (c[1]); secDyn->setBounds (c[2]); }
    r.removeFromTop (gap);

    { auto row = r.removeFromTop (168);
      auto c = split (row, { 1.55f, 1.35f });
      secMb->setBounds (c[0]); secTone->setBounds (c[1]); }
    r.removeFromTop (gap);

    { auto row = r.removeFromTop (150);
      auto c = split (row, { 1.30f, 1.60f });
      secMs->setBounds (c[0]); secOut->setBounds (c[1]); }
    r.removeFromTop (gap);

    // --- télémétrie ---
    auto m = r.removeFromTop (juce::jmax (150, r.getHeight() - 6));
    secMeters->setBounds (m);
    auto inner = m.reduced (10); inner.removeFromTop (24);

    auto top = inner.removeFromTop (56);
    auto cols = split (top, { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f }, 12);
    Readout* ro[] = { &rLufsI, &rLufsS, &rLufsM, &rTp, &rLra, &rPlr };
    for (int i = 0; i < 6; ++i) ro[i]->setBounds (cols[(size_t) i]);

    inner.removeFromTop (6);
    auto bars = split (inner, { 1.f, 1.f }, 18);
    auto left = bars[0];
    const int bh = juce::jmax (14, left.getHeight() / 5);
    for (auto& b : barMb) b.setBounds (left.removeFromTop (bh));
    barLim.setBounds (left.removeFromTop (bh));
    corr.setBounds (bars[1].removeFromTop (34));
}

void CapstoneEditor::timerCallback()
{
    const auto& t = deliveryTargets()[(size_t) (int) proc.apvts.getRawParameterValue (pid::target)->load()];
    const float lufsI = proc.meters.lufsI.load();
    const float tp    = proc.meters.truePeak.load();

    // Vert quand on est dans la cible, ambre sinon : le but n'est pas d'etre fort,
    // c'est d'etre juste. Une plateforme qui normalise annulera tout exces.
    const bool free = (int) proc.apvts.getRawParameterValue (pid::target)->load() == 0;
    auto colFor = [&] (float v, float goal, float tol)
    { return free ? col::text : (std::abs (v - goal) <= tol ? col::ok : col::meter); };

    rLufsI.set (lufsI, lufsI < -190.f ? col::text : colFor (lufsI, t.lufs, 1.0f));
    rLufsI.setSub (free ? "" : juce::String (lufsI < -190.f ? 0.f : lufsI - t.lufs, 1) + " LU / cible");
    rLufsS.set (proc.meters.lufsS.load());
    rLufsM.set (proc.meters.lufsM.load());
    rTp.set (tp, tp < -190.f ? col::text : (tp > t.dbtp + 0.05f ? col::danger : col::ok));
    rTp.setSub (free ? "" : "plafond " + juce::String (t.dbtp, 1) + " dBTP");
    rLra.set (proc.meters.lra.load());
    const float plr = (tp > -190.f && lufsI > -190.f) ? tp - lufsI : -200.f;
    rPlr.set (plr);
    rPlr.setSub (plr > -190.f ? (plr < 6.f ? "tres compresse" : plr < 10.f ? "dense" : "dynamique") : "");

    for (int b = 0; b < 4; ++b) barMb[b].setValue (proc.meters.mbGr[b].load());
    barLim.setValue (proc.meters.limGr.load());
    corr.setValue (proc.meters.correlation.load());

    if (presetBox.getSelectedId() - 1 != proc.getCurrentProgram())
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

#pragma once
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"

// --- contrôles de base (identiques d'esprit à MONOLITH) ---------------------
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 15);
        addAndMakeVisible (slider);
        caption.setText (text, juce::dontSendNotification);
        caption.setJustificationType (juce::Justification::centred);
        caption.setColour (juce::Label::textColourId, mono::col::textDim);
        caption.setFont (mono::makeFont (10.0f));
        addAndMakeVisible (caption);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    }
    void resized() override { auto r = getLocalBounds(); caption.setBounds (r.removeFromBottom (13)); slider.setBounds (r); }
    juce::Slider slider;
private:
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class Selector : public juce::Component
{
public:
    Selector (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text)
    {
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))) box.addItemList (cp->choices, 1);
        box.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (box);
        caption.setText (text, juce::dontSendNotification);
        caption.setJustificationType (juce::Justification::centred);
        caption.setColour (juce::Label::textColourId, mono::col::textDim);
        caption.setFont (mono::makeFont (10.0f));
        addAndMakeVisible (caption);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
    }
    void resized() override
    { auto r = getLocalBounds(); caption.setBounds (r.removeFromBottom (13));
      box.setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, 24)); }
    juce::ComboBox box;
private:
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text)
    { btn.setButtonText (text); btn.setClickingTogglesState (true); addAndMakeVisible (btn);
      att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, btn); }
    void resized() override { btn.setBounds (getLocalBounds().reduced (2, 6)); }
    juce::TextButton btn;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

class Section : public juce::Component
{
public:
    Section (const juce::String& t, juce::AudioProcessorValueTreeState* s = nullptr, const juce::String& powerId = {})
        : title (t)
    {
        if (s && powerId.isNotEmpty())
        {
            power = std::make_unique<juce::TextButton> ("ON");
            power->setClickingTogglesState (true);
            addAndMakeVisible (*power);
            powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (*s, powerId, *power);
        }
    }
    void add (juce::Component* c) { items.add (c); addAndMakeVisible (c); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (mono::col::panel);   g.fillRoundedRectangle (r, 5.0f);
        g.setColour (mono::col::outline); g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (mono::col::accent.withAlpha (0.85f));
        g.fillRect (r.getX() + 8.0f, r.getY() + 21.0f, 24.0f, 1.5f);
        g.setColour (mono::col::text);
        g.setFont (mono::makeFont (11.0f, true));
        g.drawText (title.toUpperCase(), (int) r.getX() + 8, (int) r.getY() + 5, getWidth() - 58, 14,
                    juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        auto head = r.removeFromTop (20);
        if (power) power->setBounds (head.removeFromRight (32).reduced (0, 2));
        r.removeFromTop (3);
        if (items.isEmpty()) return;
        const int cols = juce::jmax (1, r.getWidth() / cellW);
        const int rows = (items.size() + cols - 1) / cols;
        const int cw = r.getWidth() / cols;
        const int chh = juce::jmax (46, r.getHeight() / juce::jmax (1, rows));
        for (int i = 0; i < items.size(); ++i)
            items[i]->setBounds (r.getX() + (i % cols) * cw, r.getY() + (i / cols) * chh, cw, chh);
    }
    int cellW = 60;
private:
    juce::String title;
    juce::OwnedArray<juce::Component> items;
    std::unique_ptr<juce::TextButton> power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
};

/** Grand afficheur numérique : valeur + unité + écart à la cible. */
class Readout : public juce::Component
{
public:
    Readout (const juce::String& n, const juce::String& u) : name (n), unit (u) {}
    void set (float v, juce::Colour c = mono::col::text)
    { if (std::abs (v - value) > 0.005f || c != colour) { value = v; colour = c; repaint(); } }
    void setSub (const juce::String& s) { if (s != sub) { sub = s; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (mono::col::textDim); g.setFont (mono::makeFont (9.5f));
        g.drawText (name, r.removeFromTop (12), juce::Justification::centredLeft);
        auto v = r.removeFromTop (juce::jmax (18, r.getHeight() - (sub.isEmpty() ? 0 : 12)));
        g.setColour (colour); g.setFont (mono::makeFont (19.0f, true));
        g.drawText (value < -190.f ? juce::String ("--") : juce::String (value, 1) + " " + unit,
                    v, juce::Justification::centredLeft);
        if (sub.isNotEmpty())
        { g.setColour (mono::col::textDim); g.setFont (mono::makeFont (9.5f));
          g.drawText (sub, r, juce::Justification::centredLeft); }
    }
private:
    juce::String name, unit, sub;
    float value = -200.f;
    juce::Colour colour = mono::col::text;
};

class GrBar : public juce::Component
{
public:
    GrBar (const juce::String& n, float range_ = 12.f, juce::Colour c = mono::col::accent)
        : name (n), range (range_), colour (c) {}
    void setValue (float db) { if (std::abs (db - value) > 0.05f) { value = db; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (mono::col::textDim); g.setFont (mono::makeFont (9.5f));
        g.drawText (name, r.removeFromLeft (56), juce::Justification::centredLeft);
        auto track = r.removeFromLeft (juce::jmax (10, r.getWidth() - 42)).reduced (0, 4).toFloat();
        g.setColour (mono::col::bg);      g.fillRoundedRectangle (track, 2.f);
        g.setColour (mono::col::outline); g.drawRoundedRectangle (track, 2.f, 0.8f);
        const float amt = juce::jlimit (0.f, 1.f, std::abs (value) / range);
        if (amt > 0.001f)
            g.setColour (amt > 0.85f ? mono::col::danger : colour),
            g.fillRoundedRectangle (track.reduced (1.f).withWidth ((track.getWidth() - 2.f) * amt), 1.5f);
        g.setColour (mono::col::textDim); g.setFont (mono::makeFont (9.f));
        g.drawText (juce::String (value, 1), r, juce::Justification::centredRight);
    }
private:
    juce::String name; float range, value = 0.f; juce::Colour colour;
};

/** Corrélation de phase : -1 (opposition) à +1 (mono). */
class CorrelationMeter : public juce::Component
{
public:
    void setValue (float v) { if (std::abs (v - value) > 0.01f) { value = v; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (mono::col::textDim); g.setFont (mono::makeFont (9.5f));
        g.drawText ("CORRELATION", r.removeFromTop (12), juce::Justification::centredLeft);
        auto track = r.reduced (0, 5).toFloat();
        g.setColour (mono::col::bg);      g.fillRoundedRectangle (track, 2.f);
        g.setColour (mono::col::outline); g.drawRoundedRectangle (track, 2.f, 0.8f);
        const float mid = track.getCentreX();
        g.setColour (mono::col::outline); g.drawLine (mid, track.getY(), mid, track.getBottom(), 1.f);
        const float x = mid + value * track.getWidth() * 0.5f;
        g.setColour (value < -0.1f ? mono::col::danger : (value < 0.3f ? mono::col::meter : mono::col::ok));
        g.fillRoundedRectangle (juce::Rectangle<float> (std::min (mid, x), track.getY() + 1.f,
                                                        std::abs (x - mid), track.getHeight() - 2.f), 1.5f);
        g.setColour (mono::col::textDim); g.setFont (mono::makeFont (9.f));
        g.drawText ("-1", track.withWidth (16.f), juce::Justification::centredLeft);
        g.drawText ("+1", track.withTrimmedLeft (track.getWidth() - 16.f), juce::Justification::centredRight);
    }
private:
    float value = 1.f;
};

// ---------------------------------------------------------------------------
class CapstoneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CapstoneEditor (CapstoneProcessor&);
    ~CapstoneEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildSections();

    CapstoneProcessor& proc;
    mono::CapstoneLookAndFeel lnf;

    juce::Label titleLabel, subtitleLabel, hintLabel;
    juce::ComboBox presetBox;
    juce::TextButton bypassBtn { "BYPASS" }, resetBtn { "RAZ MESURE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    juce::OwnedArray<Section> sections;
    Section *secIn=nullptr, *secCorr=nullptr, *secDyn=nullptr, *secMb=nullptr,
            *secTone=nullptr, *secMs=nullptr, *secOut=nullptr, *secMeters=nullptr;

    Readout rLufsI { "LUFS INTEGRE", "LUFS" }, rLufsS { "COURT TERME", "LUFS" },
            rLufsM { "MOMENTANE",   "LUFS" }, rTp    { "TRUE PEAK",   "dBTP" },
            rLra   { "PLAGE (LRA)", "LU"   }, rPlr   { "PLR",         "LU"   };
    CorrelationMeter corr;
    GrBar barMb[4] { {"GRAVE"}, {"BAS-MED"}, {"HAUT-MED"}, {"AIGU"} };
    GrBar barLim { "LIMITEUR", 12.f, mono::col::danger };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CapstoneEditor)
};

#pragma once
// ============================================================================
//  CAPSTONE — Habillage graphique (sombre, accent turquoise, lisible en régie)
// ============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

namespace mono {

namespace col {
    const juce::Colour bg       { 0xFF0E1014 };
    const juce::Colour panel    { 0xFF171A21 };
    const juce::Colour panelHi  { 0xFF1E222B };
    const juce::Colour outline  { 0xFF2A2F3A };
    const juce::Colour text     { 0xFFB9C0CE };
    const juce::Colour textDim  { 0xFF6B7383 };
    const juce::Colour accent   { 0xFF4FD1C5 };   // turquoise
    const juce::Colour meter    { 0xFFE0A93A };   // ambre
    const juce::Colour ok       { 0xFF5FBF6A };   // dans la cible
    const juce::Colour danger   { 0xFFE3574B };
}

/** Construit une police en restant compatible JUCE 7 et JUCE 8. */
inline juce::Font makeFont (float height, bool bold = false)
{
   #if defined (JUCE_MAJOR_VERSION) && JUCE_MAJOR_VERSION >= 8
    auto f = juce::Font (juce::FontOptions (height));
   #else
    auto f = juce::Font (height);
   #endif
    return bold ? f.boldened() : f;
}

class CapstoneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CapstoneLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::Label::textColourId,                 col::text);
        setColour (juce::Slider::textBoxTextColourId,         col::text);
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId,        col::panelHi);
        setColour (juce::ComboBox::textColourId,              col::text);
        setColour (juce::ComboBox::outlineColourId,           col::outline);
        setColour (juce::ComboBox::arrowColourId,             col::accent);
        setColour (juce::PopupMenu::backgroundColourId,       col::panel);
        setColour (juce::PopupMenu::textColourId,             col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.35f));
        setColour (juce::TextButton::buttonColourId,          col::panelHi);
        setColour (juce::TextButton::buttonOnColourId,        col::accent);
        setColour (juce::TextButton::textColourOffId,         col::textDim);
        setColour (juce::TextButton::textColourOnId,          juce::Colours::black);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  centre = bounds.getCentre();
        const float angle  = startAngle + pos * (endAngle - startAngle);
        const float thick  = juce::jmax (2.5f, radius * 0.16f);

        // Détecte les réglages bipolaires (gain EQ, pan, transitoires) pour
        // partir du centre plutôt que du minimum : bien plus lisible.
        const double lo = s.getMinimum(), hi = s.getMaximum();
        const bool bipolar = (lo < -0.01) && (hi > 0.01) && std::abs (lo + hi) < std::abs (hi) * 0.35;
        const float originPos = bipolar ? 0.5f : 0.0f;
        const float originAng = startAngle + originPos * (endAngle - startAngle);

        // Rail
        juce::Path rail;
        rail.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f, startAngle, endAngle, true);
        g.setColour (col::outline);
        g.strokePath (rail, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Valeur
        if (std::abs (angle - originAng) > 0.001f)
        {
            juce::Path val;
            val.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f,
                               juce::jmin (originAng, angle), juce::jmax (originAng, angle), true);
            g.setColour (s.isEnabled() ? col::accent : col::textDim);
            g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Corps
        g.setColour (col::panelHi);
        g.fillEllipse (juce::Rectangle<float> (radius * 1.35f, radius * 1.35f).withCentre (centre));
        g.setColour (col::outline);
        g.drawEllipse (juce::Rectangle<float> (radius * 1.35f, radius * 1.35f).withCentre (centre), 1.0f);

        // Index
        juce::Path pointer;
        pointer.addRoundedRectangle (-1.4f, -radius * 0.68f, 2.8f, radius * 0.40f, 1.4f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
        g.setColour (col::text);
        g.fillPath (pointer);
    }

    juce::Font getLabelFont (juce::Label&) override
    { return makeFont (12.0f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::accent.withAlpha (down ? 0.85f : 1.0f)
                        : (over ? col::panelHi.brighter (0.15f) : col::panelHi));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? col::accent : col::outline);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }
};

} // namespace mono

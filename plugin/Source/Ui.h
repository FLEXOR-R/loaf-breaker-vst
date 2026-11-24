// LOAF BREAKER VST: the look (FLEXOR, 24 Nov 2026: "un style un tout petit peu pixel et dark, dans l'esthétique du
// personnage qu'elle incarne, loaf lord ... un peu gritty, noir et blanc pour le skin par défaut, garder le système de
// skin"): the skins, the pixel fonts (Silkscreen, VT323: OFL), the grain over everything, the shared drawing.
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace ui {

struct Skin {
    const char* name;
    juce::Colour bg, panel, panel2, line, ink, dim, accent, hot;
};
int skinCount();
const Skin& skin(int i);
const Skin& cur();                       // the one in use
int curIndex();
void setSkin(int i);                     // (kept in the user's settings)

juce::Font label(float h);              // Silkscreen: the labels, caps
juce::Font labelBold(float h);
juce::Font value(float h);              // VT323: the values, the numbers
juce::Image face();                      // LOAF LORD's face
juce::Image title();                     // the gothic LOAF LORD

juce::ApplicationProperties& settings();  // the user's (skin, keys, view)

// the grain: a tile of dots and noise drawn over a component, and a vignette
void grain(juce::Graphics& g, juce::Rectangle<int> r, float amount = 1.0f);
void vignette(juce::Graphics& g, juce::Rectangle<float> r);
// a panel: flat, a pixel border, its title in a tab at its top-left
void panel(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {});
// a pixel button face (on: filled)
void button(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, bool hover = false,
            float textH = 13.0f);
// text that fits: shrinks to the width
void fitText(juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Font f,
             juce::Justification j = juce::Justification::centred);

} // namespace ui

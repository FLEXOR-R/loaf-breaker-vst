// LOAF BREAKER VST: the look (FLEXOR, 24 Nov 2026: "un style un tout petit peu pixel et dark, dans l'esthétique du
// personnage qu'elle incarne, loaf lord ... un peu gritty, noir et blanc pour le skin par défaut, garder le système de
// skin"): the skins, the fonts (Barlow Condensed: OFL), the grain over everything, the shared drawing.
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace ui {

struct Skin {
    const char* name;
    juce::Colour bg, panel, panel2, line, ink, dim, accent, hot, white;
    bool fm1 = false;                    // one of the FM-1's 24
};
int skinCount();                         // the VST's own, the FM-1's 24 (from fm1First()), PERSO last
int fm1First();
int persoIndex();
constexpr int FOLLOW_FM1 = 1000;         // "COMME LE FM-1": the skin the firmware's SETUP has
const Skin& skin(int i);
const Skin& cur();                       // the one in use
int curIndex();
void setSkin(int i);                     // (kept in the user's settings)
bool fm1SkinIs(int s);                   // the firmware's skin (0..23); true: the VST follows it and it changed
Skin perso();
void setPerso(const Skin& k);

juce::Font label(float h);              // Barlow Condensed SemiBold: the labels
juce::Font labelBold(float h);          // Barlow Condensed Bold
juce::Font value(float h);              // Barlow Condensed Medium: the values, the numbers
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

// the redesign (FLEXOR, 25 Nov 2026: "je veux que tu refonte tout le vst avec cet esprit là"): a card (rounded, its chip,
// its title and what it does), the layers' colours (SUB red, PUNCH amber, GRIT green, CLICK blue, TOK violet: the same
// everywhere), the FM-1's RGB view's colour of a column, an OFF stretch's hatch
void card(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, const juce::String& sub = {},
          juce::Colour chip = juce::Colours::transparentBlack, bool lit = false);
juce::Colour layer(const juce::String& name);   // transparent: not a layer
juce::Colour rgb(float lo, float mi, float hi, float alpha = 1.0f);
void hatch(juce::Graphics& g, juce::Rectangle<float> r);
juce::String subtitle(const juce::String& tab, const juce::String& part);   // a part's few words ("" none)

} // namespace ui

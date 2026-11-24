// LOAF BREAKER VST: the computer's keyboard (FLEXOR: "de quoi mapper les touches du clavier"): every action (a slice, a
// pad, a button, a knob a step) a key of its own, chosen in the settings (KEYBOARD), kept for the user.
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

struct KeyAction {
    enum Kind { White, Black, Button, Turn };
    Kind kind;
    int index;                           // the slice, the pad, the button (LB_*), the encoder (LB_*)
    int dir;                             // Turn: +1 / -1
    juce::String name;
};

class KeyMap
{
public:
    static KeyMap& get();
    const std::vector<KeyAction>& actions() const { return list; }
    int keyOf(int action) const { return keys[(size_t) action]; }
    void assign(int action, int keyCode);  // (another action with that key loses it)
    void clear(int action);
    void defaults();
    int actionOf(int keyCode) const;
    static juce::String keyName(int keyCode);
    juce::String labelOf(KeyAction::Kind k, int index) const;   // the key shown on a pad ("" none)

private:
    KeyMap();
    void save();
    std::vector<KeyAction> list;
    std::vector<int> keys;
};

// the settings' KEYBOARD: each action, its key, ASSIGN (the next key pressed) and CLEAR
class KeyMapEditor : public juce::Component, private juce::ListBoxModel
{
public:
    KeyMapEditor();
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    juce::ListBox box { "keys", this };
    juce::TextButton reset { "PAR DEFAUT" };
    int learning = -1;
};

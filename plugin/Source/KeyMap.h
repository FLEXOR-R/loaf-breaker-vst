// LOAF BREAKER VST: the computer's keyboard (FLEXOR: "de quoi mapper les touches du clavier"; then "pouvoir utiliser
// les touches du clavier pour se déplacer facilement ... dans le séquenceur et appuyer sur entrée pour mettre une step,
// un workflow clavier souris beaucoup plus optimisé ... avec des options pour tout mapper"): every action a key with
// its modifiers (Ctrl, Shift, Alt), in its context: everywhere, the sequencer's grid, the menus. The same key can do
// one thing in the grid and another in the menus; the settings' CLAVIER sets each, kept for the user.
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

struct KeyAction {
    enum Kind { White, Black, Button, Turn, Nav, Seq, Menu };
    enum { Tempo = 7, BreakStep = 8 };   // Turn: the tempo, the playlist (not encoders)
    // Nav (everywhere): the domains, the tabs, SAVE, undo, copy, paste, the break list, the playlist, the manager, the
    // FM-1's screen
    enum { NavDomain0 = 0, NavTabNext = 8, NavTabPrev, NavSave, NavUndo, NavCopy, NavPaste, NavBreaks, NavPlaylist,
           NavManager, NavScreen, NavKeys, NavFull, NavBorderless, NavN };
    // Seq (the grid): the cursor, a step on / off, its values, the lane a step earlier / later
    enum { SeqLeft = 0, SeqRight, SeqUp, SeqDown, SeqBeatLeft, SeqBeatRight, SeqBarLeft, SeqBarRight, SeqHome, SeqEnd,
           SeqToggle, SeqClear, SeqMainUp, SeqMainDown, SeqAltUp, SeqAltDown, SeqChanceUp, SeqChanceDown,
           SeqNudgeUp, SeqNudgeDown, SeqRotLeft, SeqRotRight, SeqN };
    // Menu (the menus' cards): the setting picked, its value
    enum { MenuUp = 0, MenuDown, MenuLeft, MenuRight, MenuBigLeft, MenuBigRight, MenuOpen, MenuN };
    Kind kind;
    int index;                           // the slice, the pad, the button (LB_*), the encoder (LB_*), the command
    int dir;                             // Turn: +1 / -1
    juce::String name;
    int context() const { return kind == Seq ? 1 : kind == Menu ? 2 : 0; }   // 0 everywhere, 1 the grid, 2 the menus
};

class KeyMap
{
public:
    static KeyMap& get();
    const std::vector<KeyAction>& actions() const { return list; }
    const juce::KeyPress& keyOf(int action) const { return keys[(size_t) action]; }
    void assign(int action, const juce::KeyPress& k);   // (another action of the same context with that key loses it)
    void clear(int action);
    void defaults(bool store = true);
    // the action a key press does in a context (1 the grid, 2 the menus: theirs first, then everywhere's); -1 none
    int actionOf(const juce::KeyPress& k, int context) const;
    static juce::String keyName(const juce::KeyPress& k);
    juce::String labelOf(KeyAction::Kind k, int index) const;   // the key shown on a pad ("" none)

private:
    KeyMap();
    void save();
    std::vector<KeyAction> list;
    std::vector<juce::KeyPress> keys;
};

// the settings' CLAVIER: each action by group, its key, a click then a key (with its modifiers) sets it, a right click
// clears it
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

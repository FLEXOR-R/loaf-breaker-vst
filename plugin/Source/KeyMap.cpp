#include "KeyMap.h"
#include "Ui.h"
#include "lbcore.h"

KeyMap& KeyMap::get()
{
    static KeyMap m;
    return m;
}

KeyMap::KeyMap()
{
    for (int i = 0; i < 16; ++i) list.push_back({ KeyAction::White, i, 0, "SLICE " + juce::String(i + 1) });
    for (int i = 0; i < 11; ++i) list.push_back({ KeyAction::Black, i, 0, "PAD " + juce::String(i + 1) });
    static const char* BN[14] = { "FX", "SEL / SHIFT", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                  "PLAY", "REC", "OCT -", "OCT +" };
    for (int b : { LB_PLAY, LB_SEL, LB_SAVE, LB_REC, LB_OCTDN, LB_OCTUP, LB_HOME, LB_EDIT, LB_SEQ, LB_ENV, LB_LFO, LB_FX,
                   LB_ARP, LB_GLO })
        list.push_back({ KeyAction::Button, b, 0, BN[b] });
    static const char* EN[7] = { "ONGLET", "PARTIE", "PRESET", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4" };
    for (int e = 0; e < 7; ++e) {
        list.push_back({ KeyAction::Turn, e, -1, juce::String(EN[e]) + " -" });
        list.push_back({ KeyAction::Turn, e, +1, juce::String(EN[e]) + " +" });
    }
    list.push_back({ KeyAction::Turn, KeyAction::Tempo, -1, "TEMPO -" });   // (the header's tempo, 1 BPM)
    list.push_back({ KeyAction::Turn, KeyAction::Tempo, +1, "TEMPO +" });
    keys.assign(list.size(), 0);
    defaults();
    const auto saved = ui::settings().getUserSettings()->getValue("keys");
    if (saved.isNotEmpty()) {
        keys.assign(list.size(), 0);
        for (auto& tok : juce::StringArray::fromTokens(saved, ";", ""))
            if (tok.contains("=")) {
                const int a = tok.upToFirstOccurrenceOf("=", false, false).getIntValue();
                if (a >= 0 && a < (int) list.size())
                    keys[(size_t) a] = tok.fromFirstOccurrenceOf("=", false, false).getIntValue();
            }
    }
}

void KeyMap::defaults()
{
    keys.assign(list.size(), 0);
    const char* W = "qwertyuiasdfghjk";
    const char* B = "1234567890-";
    for (int i = 0; i < 16; ++i) keys[(size_t) i] = W[i];
    for (int i = 0; i < 11; ++i) keys[(size_t) (16 + i)] = B[i];
    for (size_t a = 0; a < list.size(); ++a) {
        const auto& k = list[a];
        if (k.kind == KeyAction::Button && k.index == LB_PLAY) keys[a] = juce::KeyPress::spaceKey;
        if (k.kind == KeyAction::Button && k.index == LB_SEL) keys[a] = 'z';
        if (k.kind == KeyAction::Button && k.index == LB_SAVE) keys[a] = 'x';
        if (k.kind == KeyAction::Turn && k.index == LB_SELECT) keys[a] = k.dir > 0 ? juce::KeyPress::downKey : juce::KeyPress::upKey;
        if (k.kind == KeyAction::Turn && k.index == LB_ALGO) keys[a] = k.dir > 0 ? juce::KeyPress::rightKey : juce::KeyPress::leftKey;
        if (k.kind == KeyAction::Turn && k.index == KeyAction::Tempo) keys[a] = k.dir > 0 ? 'm' : 'n';
        if (k.kind == KeyAction::Turn && k.index == LB_PRESET) keys[a] = k.dir > 0 ? juce::KeyPress::pageDownKey : juce::KeyPress::pageUpKey;
    }
    save();
}

void KeyMap::assign(int action, int keyCode)
{
    for (auto& k : keys)
        if (k == keyCode) k = 0;
    keys[(size_t) action] = keyCode;
    save();
}

void KeyMap::clear(int action) { keys[(size_t) action] = 0; save(); }

int KeyMap::actionOf(int keyCode) const
{
    for (size_t a = 0; a < keys.size(); ++a)
        if (keys[a] == keyCode) return (int) a;
    return -1;
}

juce::String KeyMap::keyName(int k)
{
    if (k == 0) return "-";
    if (k == juce::KeyPress::spaceKey) return "ESPACE";
    if (k == juce::KeyPress::upKey) return "HAUT";
    if (k == juce::KeyPress::downKey) return "BAS";
    if (k == juce::KeyPress::leftKey) return "GAUCHE";
    if (k == juce::KeyPress::rightKey) return "DROITE";
    if (k == juce::KeyPress::pageUpKey) return "PG HAUT";
    if (k == juce::KeyPress::pageDownKey) return "PG BAS";
    if (k == juce::KeyPress::returnKey) return "ENTREE";
    if (k == juce::KeyPress::tabKey) return "TAB";
    if (k == juce::KeyPress::backspaceKey) return "RETOUR";
    if (k >= 32 && k < 127) return juce::String::charToString((juce::juce_wchar) k).toUpperCase();
    return juce::KeyPress(k).getTextDescription().toUpperCase();
}

juce::String KeyMap::labelOf(KeyAction::Kind kind, int index) const
{
    for (size_t a = 0; a < list.size(); ++a)
        if (list[a].kind == kind && list[a].index == index && keys[a]) return keyName(keys[a]);
    return {};
}

void KeyMap::save()
{
    juce::String s;
    for (size_t a = 0; a < keys.size(); ++a)
        if (keys[a]) s << (int) a << "=" << keys[a] << ";";
    ui::settings().getUserSettings()->setValue("keys", s);
}

// ------------------------------------------------------------------------------------------------- the editor ---
KeyMapEditor::KeyMapEditor()
{
    box.setRowHeight(24);
    box.setColour(juce::ListBox::backgroundColourId, ui::cur().panel);
    addAndMakeVisible(box);
    reset.onClick = [this] { KeyMap::get().defaults(); learning = -1; box.repaint(); };
    addAndMakeVisible(reset);
    setWantsKeyboardFocus(true);
    setSize(460, 560);
}

void KeyMapEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::cur().bg);
    g.setColour(ui::cur().ink);
    g.setFont(ui::labelBold(14.0f));
    g.drawText("CLAVIER", 12, 8, 300, 22, juce::Justification::centredLeft);
    g.setColour(ui::cur().dim);
    g.setFont(ui::label(9.0f));
    g.drawText(learning >= 0 ? "APPUIE SUR UNE TOUCHE (ECHAP: ANNULER)" : "CLIQUE UNE LIGNE PUIS UNE TOUCHE", 12, 30, 400,
               14, juce::Justification::centredLeft);
}

void KeyMapEditor::resized()
{
    auto r = getLocalBounds().reduced(10);
    r.removeFromTop(40);
    reset.setBounds(r.removeFromBottom(28).removeFromRight(140));
    r.removeFromBottom(6);
    box.setBounds(r);
}

int KeyMapEditor::getNumRows() { return (int) KeyMap::get().actions().size(); }

void KeyMapEditor::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool)
{
    const auto& s = ui::cur();
    const auto& a = KeyMap::get().actions()[(size_t) row];
    g.setColour(row == learning ? s.accent : row % 2 ? s.panel : s.panel2);
    g.fillRect(0, 0, w, h);
    g.setColour(row == learning ? s.bg : s.ink);
    g.setFont(ui::label(10.0f));
    g.drawText(a.name, 10, 0, w / 2, h, juce::Justification::centredLeft);
    g.setFont(ui::value(20.0f));
    g.drawText(row == learning ? "..." : KeyMap::keyName(KeyMap::get().keyOf(row)), w / 2, 0, w / 2 - 10, h,
               juce::Justification::centredRight);
}

void KeyMapEditor::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) {
        KeyMap::get().clear(row);
        learning = -1;
    } else
        learning = row;
    box.repaint();
    repaint();
    grabKeyboardFocus();
}

bool KeyMapEditor::keyPressed(const juce::KeyPress& k)
{
    if (learning < 0) return false;
    if (k.getKeyCode() != juce::KeyPress::escapeKey) {
        int code = k.getKeyCode();
        if (code >= 'A' && code <= 'Z') code += 'a' - 'A';
        KeyMap::get().assign(learning, code);
    }
    learning = -1;
    box.repaint();
    repaint();
    return true;
}

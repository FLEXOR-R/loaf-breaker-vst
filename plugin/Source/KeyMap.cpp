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
        list.push_back({ KeyAction::Button, b, 0, juce::String("BOUTON ") + BN[b] });
    static const char* EN[7] = { "ONGLET (SELECT)", "PARTIE (ALGORITHM)", "PRESET", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4" };
    for (int e = 0; e < 7; ++e) {
        list.push_back({ KeyAction::Turn, e, -1, juce::String(EN[e]) + " -" });
        list.push_back({ KeyAction::Turn, e, +1, juce::String(EN[e]) + " +" });
    }
    list.push_back({ KeyAction::Turn, KeyAction::Tempo, -1, "TEMPO -" });
    list.push_back({ KeyAction::Turn, KeyAction::Tempo, +1, "TEMPO +" });
    list.push_back({ KeyAction::Turn, KeyAction::BreakStep, -1, "BREAK PRECEDENT (PLAYLIST)" });
    list.push_back({ KeyAction::Turn, KeyAction::BreakStep, +1, "BREAK SUIVANT (PLAYLIST)" });
    static const char* DOM[8] = { "LIVE", "SOUNDS", "PATTERN", "SONG", "MIX", "EFFECTS", "ARP", "SYSTEM" };
    for (int d = 0; d < 8; ++d) list.push_back({ KeyAction::Nav, KeyAction::NavDomain0 + d, 0, juce::String("ALLER A ") + DOM[d] });
    static const char* NAV[] = { "ONGLET SUIVANT", "ONGLET PRECEDENT", "SAUVER (SAVE)", "ANNULER (PATTERN)", "COPIER LE PATTERN",
                                 "COLLER LE PATTERN", "LISTE DES BREAKS", "PLAYLIST", "GESTION (VST, BIBLIO, FM-1)",
                                 "APERCU DU FM-1", "CLAVIER (CETTE FENETRE)", "PLEIN ECRAN", "PLEIN ECRAN SANS BORDURE" };
    for (int i = KeyAction::NavTabNext; i < KeyAction::NavN; ++i) list.push_back({ KeyAction::Nav, i, 0, NAV[i - KeyAction::NavTabNext] });
    static const char* SEQ[] = { "SEQ: PAS A GAUCHE", "SEQ: PAS A DROITE", "SEQ: LIGNE AU-DESSUS", "SEQ: LIGNE EN DESSOUS",
                                 "SEQ: UN TEMPS A GAUCHE", "SEQ: UN TEMPS A DROITE", "SEQ: UNE MESURE A GAUCHE",
                                 "SEQ: UNE MESURE A DROITE", "SEQ: PREMIER PAS", "SEQ: DERNIER PAS", "SEQ: POSER / ENLEVER LE PAS",
                                 "SEQ: ENLEVER LE PAS", "SEQ: HITS / SLICE +", "SEQ: HITS / SLICE -", "SEQ: NOTE / PITCH +",
                                 "SEQ: NOTE / PITCH -", "SEQ: CHANCE +", "SEQ: CHANCE -", "SEQ: NUDGE +", "SEQ: NUDGE -",
                                 "SEQ: LIGNE UN PAS PLUS TOT", "SEQ: LIGNE UN PAS PLUS TARD" };
    for (int i = 0; i < KeyAction::SeqN; ++i) list.push_back({ KeyAction::Seq, i, 0, SEQ[i] });
    static const char* MEN[] = { "MENUS: REGLAGE AU-DESSUS", "MENUS: REGLAGE EN DESSOUS", "MENUS: VALEUR -", "MENUS: VALEUR +",
                                 "MENUS: VALEUR - (GRAND PAS)", "MENUS: VALEUR + (GRAND PAS)", "MENUS: OUVRIR LA LISTE" };
    for (int i = 0; i < KeyAction::MenuN; ++i) list.push_back({ KeyAction::Menu, i, 0, MEN[i] });
    keys.assign(list.size(), {});
    defaults(false);                     // (the defaults, then the player's own over them: not saved over first)
    const auto saved = ui::settings().getUserSettings()->getValue("keys3");
    if (saved.isNotEmpty()) {            // (each "ACTION NAME=DESCRIPTION|": by name, so new actions never shift the others;
                                         // an action the save doesn't name keeps its default)
        for (auto& tok : juce::StringArray::fromTokens(saved, "|", ""))
            if (tok.contains("=")) {
                const auto name = tok.upToLastOccurrenceOf("=", false, false);
                for (size_t a = 0; a < list.size(); ++a)
                    if (list[a].name == name) {
                        const auto d = tok.fromLastOccurrenceOf("=", false, false);
                        keys[a] = d.isEmpty() ? juce::KeyPress() : juce::KeyPress::createFromDescription(d);
                    }
            }
    }
}

void KeyMap::defaults(bool store)
{
    using K = juce::KeyPress;
    using M = juce::ModifierKeys;
    keys.assign(list.size(), {});
    const char* W = "qwertyuiasdfghjk";
    const char* B = "1234567890-";
    for (int i = 0; i < 16; ++i) keys[(size_t) i] = K(W[i]);
    for (int i = 0; i < 11; ++i) keys[(size_t) (16 + i)] = K(B[i]);
    const int ctrl = M::commandModifier, shift = M::shiftModifier, alt = M::altModifier;
    for (size_t a = 0; a < list.size(); ++a) {
        const auto& k = list[a];
        auto set = [&](int code, int mods = 0) { keys[a] = K(code, M(mods), 0); };
        if (k.kind == KeyAction::Button && k.index == LB_PLAY) set(K::spaceKey);
        if (k.kind == KeyAction::Button && k.index == LB_SEL) set('z');
        if (k.kind == KeyAction::Turn && k.index == LB_PRESET) set(k.dir > 0 ? K::pageDownKey : K::pageUpKey);
        if (k.kind == KeyAction::Turn && k.index == KeyAction::Tempo) set(k.dir > 0 ? 'm' : 'n');
        if (k.kind == KeyAction::Turn && k.index == KeyAction::BreakStep) set(k.dir > 0 ? 'v' : 'c');
        if (k.kind == KeyAction::Nav) {
            if (k.index < 8) set(K::F1Key + k.index);
            switch (k.index) {
            case KeyAction::NavTabNext: set(K::tabKey); break;
            case KeyAction::NavTabPrev: set(K::tabKey, shift); break;
            case KeyAction::NavSave: set('s', ctrl); break;
            case KeyAction::NavUndo: set('z', ctrl); break;
            case KeyAction::NavCopy: set('c', ctrl); break;
            case KeyAction::NavPaste: set('v', ctrl); break;
            case KeyAction::NavBreaks: set('b', ctrl); break;
            case KeyAction::NavPlaylist: set('p', ctrl); break;
            case KeyAction::NavManager: set('g', ctrl); break;
            case KeyAction::NavScreen: set(K::F9Key); break;
            case KeyAction::NavKeys: set('k', ctrl); break;
            case KeyAction::NavFull: set(K::F11Key); break;
            case KeyAction::NavBorderless: set(K::F11Key, shift); break;
            default: break;
            }
        }
        if (k.kind == KeyAction::Seq)
            switch (k.index) {
            case KeyAction::SeqLeft: set(K::leftKey); break;
            case KeyAction::SeqRight: set(K::rightKey); break;
            case KeyAction::SeqUp: set(K::upKey); break;
            case KeyAction::SeqDown: set(K::downKey); break;
            case KeyAction::SeqBeatLeft: set(K::leftKey, shift); break;
            case KeyAction::SeqBeatRight: set(K::rightKey, shift); break;
            case KeyAction::SeqBarLeft: set(K::leftKey, ctrl); break;
            case KeyAction::SeqBarRight: set(K::rightKey, ctrl); break;
            case KeyAction::SeqHome: set(K::homeKey); break;
            case KeyAction::SeqEnd: set(K::endKey); break;
            case KeyAction::SeqToggle: set(K::returnKey); break;
            case KeyAction::SeqClear: set(K::deleteKey); break;
            case KeyAction::SeqMainUp: set(K::upKey, alt); break;
            case KeyAction::SeqMainDown: set(K::downKey, alt); break;
            case KeyAction::SeqAltUp: set(K::upKey, shift); break;
            case KeyAction::SeqAltDown: set(K::downKey, shift); break;
            case KeyAction::SeqChanceUp: set(K::upKey, ctrl); break;
            case KeyAction::SeqChanceDown: set(K::downKey, ctrl); break;
            case KeyAction::SeqNudgeUp: set(K::rightKey, alt); break;
            case KeyAction::SeqNudgeDown: set(K::leftKey, alt); break;
            case KeyAction::SeqRotLeft: set('['); break;
            case KeyAction::SeqRotRight: set(']'); break;
            default: break;
            }
        if (k.kind == KeyAction::Menu)
            switch (k.index) {
            case KeyAction::MenuUp: set(K::upKey); break;
            case KeyAction::MenuDown: set(K::downKey); break;
            case KeyAction::MenuLeft: set(K::leftKey); break;
            case KeyAction::MenuRight: set(K::rightKey); break;
            case KeyAction::MenuBigLeft: set(K::leftKey, shift); break;
            case KeyAction::MenuBigRight: set(K::rightKey, shift); break;
            case KeyAction::MenuOpen: set(K::returnKey); break;
            default: break;
            }
    }
    if (store) save();
}

static juce::KeyPress norm(const juce::KeyPress& k)   // (a letter as its small one; Ctrl, Shift, Alt kept)
{
    int code = k.getKeyCode();
    if (code >= 'A' && code <= 'Z') code += 'a' - 'A';
    const auto m = k.getModifiers().withOnlyMouseButtons().withFlags(k.getModifiers().getRawFlags() &
        (juce::ModifierKeys::shiftModifier | juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier | juce::ModifierKeys::commandModifier));
    return juce::KeyPress(code, m, 0);
}

void KeyMap::assign(int action, const juce::KeyPress& k)
{
    const auto n = norm(k);
    const int ctx = list[(size_t) action].context();
    for (size_t a = 0; a < keys.size(); ++a)
        if (list[a].context() == ctx && keys[a] == n) keys[a] = {};
    keys[(size_t) action] = n;
    save();
}

void KeyMap::clear(int action) { keys[(size_t) action] = {}; save(); }

int KeyMap::actionOf(const juce::KeyPress& k, int context) const
{
    const auto n = norm(k);
    for (int pass = 0; pass < 2; ++pass)  // (the context's own first, then everywhere's)
        for (size_t a = 0; a < keys.size(); ++a)
            if (keys[a].isValid() && keys[a] == n && list[a].context() == (pass ? 0 : context) && (pass || context))
                return (int) a;
    return -1;
}

juce::String KeyMap::keyName(const juce::KeyPress& k)
{
    using K = juce::KeyPress;
    if (! k.isValid()) return "-";
    const int c = k.getKeyCode();
    juce::String base;
    if (c == K::spaceKey) base = "ESPACE";
    else if (c == K::upKey) base = "HAUT";
    else if (c == K::downKey) base = "BAS";
    else if (c == K::leftKey) base = "GAUCHE";
    else if (c == K::rightKey) base = "DROITE";
    else if (c == K::pageUpKey) base = "PG HAUT";
    else if (c == K::pageDownKey) base = "PG BAS";
    else if (c == K::returnKey) base = "ENTREE";
    else if (c == K::tabKey) base = "TAB";
    else if (c == K::backspaceKey) base = "RETOUR";
    else if (c == K::deleteKey) base = "SUPPR";
    else if (c == K::homeKey) base = "DEBUT";
    else if (c == K::endKey) base = "FIN";
    else if (c >= 32 && c < 127) base = juce::String::charToString((juce::juce_wchar) c).toUpperCase();
    else base = K(c).getTextDescription().toUpperCase();
    const auto m = k.getModifiers();
    juce::String s;
    if (m.isCommandDown() || m.isCtrlDown()) s << "CTRL+";
    if (m.isAltDown()) s << "ALT+";
    if (m.isShiftDown()) s << "SHIFT+";
    return s + base;
}

juce::String KeyMap::labelOf(KeyAction::Kind kind, int index) const
{
    for (size_t a = 0; a < list.size(); ++a)
        if (list[a].kind == kind && list[a].index == index && keys[a].isValid()) return keyName(keys[a]);
    return {};
}

void KeyMap::save()
{
    juce::String s;
    for (size_t a = 0; a < keys.size(); ++a)  // (every action, a cleared one with nothing after its =)
        s << list[a].name << "=" << (keys[a].isValid() ? keys[a].getTextDescription() : juce::String()) << "|";
    ui::settings().getUserSettings()->setValue("keys3", s);
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
    setSize(560, 680);
}

void KeyMapEditor::paint(juce::Graphics& g)
{
    g.fillAll(ui::cur().bg);
    g.setColour(ui::cur().ink);
    g.setFont(ui::labelBold(16.0f));
    g.drawText("CLAVIER", 12, 8, 300, 22, juce::Justification::centredLeft);
    g.setColour(ui::cur().dim);
    g.setFont(ui::label(11.0f));
    g.drawText(learning >= 0 ? "APPUIE SUR UNE TOUCHE, AVEC CTRL / SHIFT / ALT SI TU VEUX (ECHAP: ANNULER)"
                             : "CLIC: CHANGER LA TOUCHE.  CLIC DROIT: L'ENLEVER.  SEQ: LA GRILLE, MENUS: LES CARTES.", 12, 30, 540,
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
    g.setFont(ui::label(12.0f));
    g.drawText(a.name, 10, 0, w * 2 / 3, h, juce::Justification::centredLeft);
    g.setFont(ui::value(17.0f));
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
    if (k.getKeyCode() != juce::KeyPress::escapeKey)
        KeyMap::get().assign(learning, k);
    learning = -1;
    box.repaint();
    repaint();
    return true;
}

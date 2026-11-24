#include "PluginEditor.h"
#include "Ui.h"
#include <set>

using LBE = LoafBreakerEditor;
static constexpr int W = 1440, H = 900;

// the domains, as the FM-1's buttons reach them
struct Domain { const char* name; int button; };
static const Domain DOMS[] = { { "LIVE", LB_HOME }, { "SOUNDS", LB_EDIT }, { "PATTERN", LB_SEQ }, { "SONG", LB_ENV },
                               { "MIX", LB_LFO }, { "EFFECTS", LB_FX }, { "ARP", LB_ARP }, { "SYSTEM", LB_GLO } };
static constexpr int NDOMS = 8;

// ----------------------------------------------------------------------------------------------------- parts ---
struct Part : juce::Component
{
    explicit Part(LBE& e) : ed(e) {}
    LBE& ed;
    const ui::Skin& s() const { return ui::cur(); }
};

// a clickable area drawn as a pixel button
struct PixButton : Part
{
    PixButton(LBE& e, juce::String t, std::function<void(bool)> f) : Part(e), text(std::move(t)), act(std::move(f)) {}
    juce::String text;
    std::function<void(bool)> act;      // down / up
    bool on = false, down = false;
    float th = 13.0f;
    void paint(juce::Graphics& g) override { ui::button(g, getLocalBounds().toFloat(), text, on || down, isMouseOver(), th); }
    void mouseDown(const juce::MouseEvent&) override { down = true; repaint(); if (act) act(true); }
    void mouseUp(const juce::MouseEvent&) override { down = false; repaint(); if (act) act(false); }
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
};

// --------------------------------------------------------------------------------------------------- header ---
struct Header : Part
{
    explicit Header(LBE& e) : Part(e)
    {
        play = std::make_unique<PixButton>(e, "PLAY", [&e](bool d) { if (d) e.tap(LB_PLAY); });
        play->th = 16.0f;
        addAndMakeVisible(*play);
        menu = std::make_unique<PixButton>(e, "MENU", [&e](bool d) { if (d) e.menu(); });
        addAndMakeVisible(*menu);
        sync = std::make_unique<PixButton>(e, "SYNC", [&e](bool d) {
            if (! d) return;
            auto* p = e.proc.params.getParameter("sync");
            p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
        });
        addAndMakeVisible(*sync);
    }
    std::unique_ptr<PixButton> play, menu, sync;
    float vl = 0, vr = 0, hl = 0, hr = 0, tdrag = 0.0f;
    bool tempoHover = false, tempoDrag = false;
    // the tempo: dragged up / down or the wheel, 1 BPM a step (SELECT never touches it in the VST)
    static juce::Rectangle<float> tempoArea() { return { 500, 8, 136, 64 }; }
    void tempo(int n) { if (n) ed.proc.send(LoafBreakerProcessor::CmdTempo, 0, n); }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const bool h = tempoArea().contains(e.position);
        if (h != tempoHover) { tempoHover = h; setMouseCursor(h ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor); repaint(); }
    }
    void mouseExit(const juce::MouseEvent&) override { if (tempoHover) { tempoHover = false; setMouseCursor(juce::MouseCursor::NormalCursor); repaint(); } }
    void mouseDown(const juce::MouseEvent& e) override { tempoDrag = tempoArea().contains(e.position); tdrag = 0.0f; }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! tempoDrag) return;
        const float y = (float) -e.getDistanceFromDragStartY();
        const int n = (int) ((y - tdrag) / 6.0f);
        if (n != 0) { tdrag += n * 6.0f; tempo(n); }
    }
    void mouseUp(const juce::MouseEvent&) override { tempoDrag = false; }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (tempoArea().contains(e.position)) tempo(w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0);
    }
    void resized() override
    {
        play->setBounds(380, 18, 110, 48);
        sync->setBounds(640, 46, 70, 20);
        menu->setBounds(W - 96, 22, 80, 40);
    }
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        auto r = getLocalBounds().toFloat();
        g.setColour(sk.bg);
        g.fillRect(r);
        // LOAF LORD's face, in the skin's light
        auto f = ui::face();
        if (f.isValid()) {
            g.setOpacity(0.95f);
            g.drawImage(f, juce::Rectangle<float>(10, 4, 78, 78), juce::RectanglePlacement::centred);
            g.setOpacity(1.0f);
        }
        g.setColour(sk.hot);
        g.setFont(ui::labelBold(30.0f));
        g.drawText("LOAF BREAKER", 98, 12, 280, 36, juce::Justification::centredLeft);
        g.setColour(sk.dim);
        g.setFont(ui::label(10.0f));
        g.drawText("BY LOAF LORD  -  BREAKS, KICKS, CHAOS", 100, 48, 280, 14, juce::Justification::centredLeft);
        // tempo
        const auto& m = ed.m;
        g.setColour(sk.dim);
        g.setFont(ui::label(11.0f));
        g.drawText(tempoHover ? "TEMPO  (GLISSER)" : "TEMPO", 510, 14, 120, 12, juce::Justification::centredLeft);
        if (tempoHover) { g.setColour(sk.line); g.drawRect(tempoArea(), 1.0f); }
        g.setColour(sk.hot);
        g.setFont(ui::value(46.0f));
        g.drawText(juce::String(m.bpm, 1), 506, 24, 130, 44, juce::Justification::centredLeft);
        sync->on = ed.proc.params.getRawParameterValue("sync")->load() > 0.5f;
        play->on = m.playing != 0;
        play->text = m.playing ? "STOP" : "PLAY";
        // the break and the pattern
        g.setColour(sk.dim);
        g.setFont(ui::label(11.0f));
        g.drawText("BREAK", 730, 14, 200, 12, juce::Justification::centredLeft);
        g.drawText("PATTERN", 950, 14, 80, 12, juce::Justification::centredLeft);
        g.setColour(sk.ink);
        ui::fitText(g, juce::String(m.brk), { 728, 26, 210, 40 }, ui::value(38.0f), juce::Justification::centredLeft);
        g.setFont(ui::value(38.0f));
        g.drawText("P" + juce::String(m.pattern).paddedLeft('0', 2), 948, 26, 80, 40, juce::Justification::centredLeft);
        // the FM-1's message, or the VST's own
        juce::String a = m.say[0], b = m.say[1];
        if (juce::Time::getMillisecondCounterHiRes() < ed.toastUntil) { a = ed.toastText; b = {}; }
        juce::Rectangle<float> msg(1036, 14, 210, 56);
        g.setColour(sk.panel);
        g.fillRect(msg);
        g.setColour(sk.line);
        g.drawRect(msg, 1.0f);
        g.setColour(sk.accent);
        ui::fitText(g, a, msg.withHeight(28).reduced(6, 0), ui::label(11.0f));
        g.setColour(sk.ink);
        ui::fitText(g, b, msg.withTrimmedTop(28).reduced(6, 0), ui::value(22.0f));
        // the meters
        for (int c = 0; c < 2; ++c) {
            const float v = c ? vr : vl, hld = c ? hr : hl;
            juce::Rectangle<float> bar(1258.0f + c * 16.0f, 12, 12, 60);
            const int segs = 15;
            for (int i = 0; i < segs; ++i) {
                const float t = (float) i / segs;
                auto seg = juce::Rectangle<float>(bar.getX(), bar.getBottom() - (i + 1) * 4.0f, 12, 3);
                const bool lit = v > t;
                g.setColour(lit ? (t > 0.85f ? sk.hot : sk.accent) : sk.line);
                if (std::abs(hld - t) < 1.0f / segs) g.setColour(sk.hot);
                g.fillRect(seg);
            }
        }
        g.setColour(sk.line);
        g.fillRect(0.0f, r.getBottom() - 1.0f, r.getWidth(), 1.0f);
    }
    void meters(float l, float r)
    {
        auto toV = [](float x) { return juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(x, -48.0f) + 48.0f) / 48.0f); };
        vl = juce::jmax(toV(l), vl * 0.85f);
        vr = juce::jmax(toV(r), vr * 0.85f);
        hl = juce::jmax(vl, hl - 0.01f);
        hr = juce::jmax(vr, hr - 0.01f);
    }
};

// ------------------------------------------------------------------------------------------ categories, tabs ---
struct Categories : Part
{
    explicit Categories(LBE& e) : Part(e) {}
    int hover = -1;
    juce::Rectangle<float> cell(int i) const { return { 10.0f + i * 140.0f, 6.0f, 134.0f, 32.0f }; }
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        g.setColour(sk.bg);
        g.fillAll();
        for (int i = 0; i < NDOMS; ++i)
            ui::button(g, cell(i), DOMS[i].name, juce::String(ed.m.dom) == DOMS[i].name, hover == i, 14.0f);
        // the page's path, right
        juce::String path = juce::String(ed.m.dom);
        if (juce::String(ed.m.label) != ed.m.dom && ed.m.label[0]) path << "  /  " << ed.m.label;
        if (ed.m.tab_on >= 0 && ed.m.tab_on < ed.m.ntabs) path << "  /  " << ed.m.tabs[ed.m.tab_on];
        if (ed.m.part[0]) path << "  /  " << ed.m.part;
        g.setColour(sk.dim);
        ui::fitText(g, path, { 1140.0f, 6.0f, 290.0f, 32.0f }, ui::label(10.0f), juce::Justification::centredRight);
    }
    void mouseMove(const juce::MouseEvent& e) override { int h = hit(e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    int hit(juce::Point<float> p) const { for (int i = 0; i < NDOMS; ++i) if (cell(i).contains(p)) return i; return -1; }
    void mouseDown(const juce::MouseEvent& e) override { int h = hit(e.position); if (h >= 0) ed.goDomain(h); }
};

struct Tabs : Part
{
    explicit Tabs(LBE& e) : Part(e) {}
    std::vector<std::pair<juce::Rectangle<float>, int>> cells;   // (a tab, or -1..-3 the sources, -10 / -11 the part)
    int hover = -9999;
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        g.setColour(sk.bg);
        g.fillAll();
        cells.clear();
        float x = 10.0f;
        if (juce::String(ed.m.dom) == "SOUNDS") {        // the sources: KICK, DRUMS, BREAK
            static const char* SRC[3] = { "KICK", "DRUMS", "BREAK" };
            for (int i = 0; i < 3; ++i) {
                juce::Rectangle<float> r(x, 4.0f, 82.0f, 26.0f);
                ui::button(g, r, SRC[i], juce::String(ed.m.label) == SRC[i], hover == -1 - i, 11.0f);
                cells.push_back({ r, -1 - i });
                x += 86.0f;
            }
            g.setColour(sk.line);
            g.fillRect(x + 4.0f, 6.0f, 2.0f, 22.0f);
            x += 14.0f;
        }
        for (int i = 0; i < ed.m.ntabs; ++i) {
            const juce::String t = ed.m.tabs[i];
            const float w = juce::jmax(74.0f, juce::GlyphArrangement::getStringWidth(ui::label(11.0f), t) + 26.0f);
            juce::Rectangle<float> r(x, 4.0f, w, 26.0f);
            if (r.getRight() > 1030.0f) break;
            ui::button(g, r, t, i == ed.m.tab_on, hover == i, 11.0f);
            cells.push_back({ r, i });
            x += w + 4.0f;
        }
        // the tools, right: SHIFT (SEL held), PRESET < >, SAVE, OCT - +
        static const char* T[7] = { "SHIFT", "<", "PRESET", ">", "SAVE", "OCT -", "OCT +" };
        for (int i = 0; i < 7; ++i) {
            if (i == 2) {
                g.setColour(sk.dim);
                ui::fitText(g, "PRESET", tool(i), ui::label(11.0f));
                continue;
            }
            ui::button(g, tool(i), T[i], (i == 0 && ed.m.sel) || (i == 0 && shiftHeld), hover == -20 - i, 11.0f);
            cells.push_back({ tool(i), -20 - i });
        }
    }
    bool shiftHeld = false;
    juce::Rectangle<float> tool(int i) const
    {
        static const float X[7] = { 1040, 1110, 1140, 1194, 1230, 1300, 1366 }, Wd[7] = { 64, 28, 52, 28, 64, 60, 60 };
        return { X[i], 4.0f, Wd[i], 26.0f };
    }
    int hit(juce::Point<float> p) const { for (auto& c : cells) if (c.first.contains(p)) return c.second; return -9999; }
    void mouseMove(const juce::MouseEvent& e) override { int h = hit(e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -9999; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int h = hit(e.position);
        if (h >= 0 && ed.m.tab_on >= 0)
            ed.turn(LB_SELECT, h - ed.m.tab_on);
        else if (h >= -3 && h <= -1) {
            static const char* SRC[3] = { "KICK", "DRUMS", "BREAK" };
            ed.goSource(SRC[-1 - h]);
        } else if (h == -20) { shiftHeld = true; ed.hold(LB_SEL, true); repaint(); }
        else if (h == -21 || h == -23) ed.turn(LB_PRESET, h == -21 ? -1 : 1);
        else if (h == -24) ed.tap(LB_SAVE);
        else if (h == -25 || h == -26) { octHeld = h == -25 ? LB_OCTDN : LB_OCTUP; ed.hold(octHeld, true); }
    }
    int octHeld = -1;
    void mouseUp(const juce::MouseEvent&) override
    {
        if (shiftHeld) { shiftHeld = false; ed.hold(LB_SEL, false); repaint(); }
        if (octHeld >= 0) { ed.hold(octHeld, false); octHeld = -1; }
    }
};

// --------------------------------------------------------------------------------------------------- the wave ---
struct WavePanel : Part
{
    explicit WavePanel(LBE& e) : Part(e) {}
    int held = -1;
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced(14.0f, 16.0f).withTrimmedBottom(26.0f); }
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        auto r = getLocalBounds().toFloat();
        ui::panel(g, r.reduced(1.0f), "BREAK  /  " + juce::String(ed.m.brk));
        auto a = area();
        // the poster's title, faint, behind
        auto t = ui::title();
        if (t.isValid()) {
            g.setOpacity(0.05f);
            g.drawImage(t, a.withSizeKeepingCentre(a.getHeight() * 2.5f, a.getHeight()), juce::RectanglePlacement::centred);
            g.setOpacity(1.0f);
        }
        const auto& w = ed.wave;
        const int n = (int) w.cols.size() / 4;
        const float mid = a.getCentreY(), half = a.getHeight() * 0.5f - 4.0f;
        // the playhead: where the break's playing, from the step (two bars of 16)
        const float play = ed.m.playing && ed.m.step >= 0 ? (float) (ed.m.step % 32) / 32.0f : -1.0f;
        if (n > 0) {
            for (float x = 0; x < a.getWidth(); x += 3.0f) {   // pixel bars: 2 px wide, a pixel apart
                const int c = juce::jlimit(0, n - 1, (int) (x / a.getWidth() * n));
                const float pk = w.cols[(size_t) (4 * c)], lo = w.cols[(size_t) (4 * c + 1)],
                            mi = w.cols[(size_t) (4 * c + 2)], hi = w.cols[(size_t) (4 * c + 3)];
                const float tot = juce::jmax(1e-3f, lo + mi + hi);
                // the colour: the bass dark, the highs bright (the skin's ink to its hot white or accent)
                juce::Colour col = sk.ink.interpolatedWith(sk.accent, juce::jlimit(0.0f, 1.0f, (hi + 0.5f * mi) / tot));
                if (play >= 0.0f && x / a.getWidth() > play) col = col.withMultipliedAlpha(0.55f);
                g.setColour(col);
                const float hgt = juce::jmax(1.0f, pk * half);
                for (float y = -hgt; y < hgt; y += 3.0f)        // stepped: a pixel grid
                    g.fillRect(a.getX() + x, mid + y, 2.0f, 2.0f);
            }
        }
        // the slices
        for (size_t i = 0; i < w.slices.size(); ++i) {
            const float x = a.getX() + w.slices[i] * a.getWidth();
            const bool now = (int) i == ed.m.slice || (int) i == held;
            g.setColour(now ? sk.hot : sk.line.brighter(0.25f));
            g.fillRect(x, a.getY(), now ? 2.0f : 1.0f, a.getHeight());
            g.setColour(now ? sk.hot : sk.dim);
            g.setFont(ui::label(11.0f));
            g.drawText(juce::String((int) i + 1), (int) x + 3, (int) a.getY() + 2, 24, 12, juce::Justification::centredLeft);
        }
        if (play >= 0.0f) {
            g.setColour(sk.hot);
            g.fillRect(a.getX() + play * a.getWidth(), a.getY() - 4.0f, 2.0f, a.getHeight() + 8.0f);
        }
        // the 32 steps, the one playing lit
        const float sw = a.getWidth() / 32.0f;
        for (int i = 0; i < 32; ++i) {
            juce::Rectangle<float> c(a.getX() + i * sw + 1.0f, a.getBottom() + 8.0f, sw - 2.0f, 10.0f);
            const bool now = ed.m.playing && ed.m.step >= 0 && ed.m.step % 32 == i;
            g.setColour(now ? sk.hot : i % 4 == 0 ? sk.line.brighter(0.3f) : sk.line);
            g.fillRect(c);
        }
    }
    int sliceAt(float x) const
    {
        const auto a = area();
        const float t = (x - a.getX()) / a.getWidth();
        int k = -1;
        for (size_t i = 0; i < ed.wave.slices.size(); ++i)
            if (t >= ed.wave.slices[i]) k = (int) i;
        return k;
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        held = sliceAt(e.position.x);
        if (held >= 0 && held < 16) ed.key(false, held, true);
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (held >= 0 && held < 16) ed.key(false, held, false);
        held = -1;
        repaint();
    }
};

// ------------------------------------------------------------------------------------------- the page as menus ---
// (FLEXOR, 24 Nov 2026: "des vrais menus, pas juste quelque chose pour contrôler le petit aperçu"; "des crans plus ou
// moins rapprochés selon s'il y a 4 options ou 99 ... pas de potard qui tourne dans le vide une fois le dernier cran
// atteint"): every part of the tab at once, a card each, each setting drawn after the values it can take (the firmware
// looked at ahead: lb_options): two, a switch; a few short ones, buttons side by side; names, a list; numbers, a slider
// with its ends, its steps as wide as their count allows
namespace menus {
enum Kind { Unknown, Fixed, Toggle, Segments, List, Slider };
static bool numeric(const juce::String& s)
{
    auto t = s.trimStart();
    if (t.startsWithChar('+') || t.startsWithChar('-') || t.startsWithChar('X')) t = t.substring(1);
    return t.isNotEmpty() && juce::CharacterFunctions::isDigit(t[0]);
}
static bool hasDigit(const juce::String& s) { return s.containsAnyOf("0123456789"); }
static bool nameAndNumber(const juce::String& s)   // "KICK 4", "DROP 2", "P12": a name, its number (a list of things)
{
    const auto w = s.upToLastOccurrenceOf(" ", false, false);
    const auto n = s.fromLastOccurrenceOf(" ", false, false);
    return (w.isNotEmpty() && ! hasDigit(w) && n.containsOnly("0123456789")) || (s.length() <= 3 && s[0] == 'P' && s.substring(1).containsOnly("0123456789"));
}
static Kind kindOf(const LBE::Opts& o)
{
    const int n = o.v.size();
    if (n <= 1) return Fixed;
    if (n == 2) return Toggle;
    int num = 0, dig = 0, named = 0, longest = 0;
    for (auto& s : o.v) {
        num += numeric(s) ? 1 : 0;
        dig += hasDigit(s) ? 1 : 0;
        named += nameAndNumber(s) ? 1 : 0;
        longest = juce::jmax(longest, s.length());
    }
    if (n <= 5 && longest <= 8) return Segments;
    if (named * 10 >= n * 5) return List;            // (things to pick: kicks, drops, patterns)
    if ((num * 10 >= n * 7 || dig * 10 >= n * 8) && n > 5) return Slider;   // (an amount: 0-100 %, Hz, ms, steps)
    return List;
}
} // namespace menus

struct MenuContent : Part
{
    explicit MenuContent(LBE& e) : Part(e) {}
    struct Param {
        int part = 0, knob = 0;
        juce::String key, name, val;
        juce::Rectangle<float> row, ctl;
        menus::Kind kind = menus::Unknown;
        int cur = -1, n = 0;
        bool wrap = false;
    };
    struct Card { juce::String title; juce::Rectangle<float> r; bool live = false; std::vector<Param> ps; };
    std::vector<Card> cards;
    int hoverCard = -1, hoverParam = -1;
    // a drag on a slider: the value it started at, the pixels a step
    Param* dragP = nullptr;
    int dragFrom = 0, dragSent = 0;
    float pxStep = 8.0f;

    int availW = 1400;
    static constexpr float CARD_W = 440.0f, ROW_H = 44.0f, HEAD_H = 30.0f, GAP = 10.0f;

    // the cards again from the scan (lb_scan's order: the part shown on the FM-1 first; kept in the page's first order)
    void rebuild()
    {
        cards.clear();
        const auto& ps = ed.parts.parts;
        const int n = (int) ps.size();
        if (n == 0) { setSize(availW, 10); repaint(); return; }
        // the natural order: the round started where the FM-1 is; shown from the page's first part when known
        int first = 0;
        const auto ord = order.find(ed.pageKey());
        if (ord != order.end()) {
            for (int i = 0; i < n; ++i) if (ident(ps[(size_t) i]) == ord->second) { first = i; break; }
        } else if (n > 1)
            order[ed.pageKey()] = ident(ps[0]);
        const float avail = (float) availW - 4.0f;
        const int cols = juce::jmax(1, (int) ((avail + GAP) / (CARD_W + GAP)));   // (three, at full width: a card never wider)
        const float cw = (avail - GAP * (cols - 1)) / cols;
        float y = 2.0f, rowH = 0.0f;
        for (int j = 0; j < n; ++j) {
            const int i = (first + j) % n;
            const auto& p = ps[(size_t) i];
            Card c;
            c.title = p.part[0] ? juce::String(p.part) : n == 1 ? tabName() : juce::String("PLUS");
            c.live = i == 0 && n > 1;
            int rows = 0;
            for (int k = 0; k < 4; ++k) if (p.kname[k][0]) rows++;
            const int col = j % cols;
            if (col == 0 && j) { y += rowH + GAP; rowH = 0.0f; }
            const float h = HEAD_H + juce::jmax(1, rows) * ROW_H + 10.0f;
            c.r = { 2.0f + col * (cw + GAP), y + 8.0f, cw, h };
            rowH = juce::jmax(rowH, h + 8.0f);
            float ry = c.r.getY() + HEAD_H - 4.0f;
            for (int k = 0; k < 4; ++k) {
                if (! p.kname[k][0]) continue;
                Param q;
                q.part = i;
                q.knob = k;
                q.name = p.kname[k];
                q.val = p.kval[k];
                q.key = ed.optKey(p, k);
                q.row = { c.r.getX() + 12.0f, ry, c.r.getWidth() - 24.0f, ROW_H - 6.0f };
                q.ctl = q.row.withTrimmedLeft(juce::jmin(130.0f, q.row.getWidth() * 0.38f));
                refreshParam(q);
                c.ps.push_back(q);
                ry += ROW_H;
            }
            cards.push_back(std::move(c));
        }
        setSize(availW, (int) (y + rowH + 12.0f));
        dragP = nullptr;
        repaint();
    }
    std::map<juce::String, juce::String> order;   // the page | its first part
    static juce::String ident(const lb_part_t& p)
    {
        juce::String s(p.part);
        for (int k = 0; k < 4; ++k) s << "|" << p.kname[k];
        return s;
    }
    juce::String tabName() const { return ed.m.tab_on >= 0 && ed.m.tab_on < ed.m.ntabs ? juce::String(ed.m.tabs[ed.m.tab_on]) : juce::String(ed.m.dom); }
    void refreshParam(Param& q)
    {
        auto it = ed.opts.find(q.key);
        if (it == ed.opts.end()) { q.kind = menus::Unknown; ed.wantOptions(q.part, q.knob); return; }
        const auto& o = it->second;
        q.n = o.v.size();
        q.wrap = o.wrap;
        q.cur = o.v.indexOf(q.val);
        if (q.cur < 0) { ed.opts.erase(it); q.kind = menus::Unknown; ed.wantOptions(q.part, q.knob); return; }   // (the list moved on)
        q.kind = menus::kindOf(o);
    }
    // the values only (the same parts): no relayout
    bool sameShape() const
    {
        const auto& ps = ed.parts.parts;
        size_t total = 0;
        for (auto& c : cards) total += c.ps.size();
        size_t want = 0;
        for (auto& p : ps) for (int k = 0; k < 4; ++k) if (p.kname[k][0]) want++;
        if (total != want || cards.size() != ps.size()) return false;
        for (auto& c : cards) for (auto& q : c.ps) {
            if (q.part >= (int) ps.size()) return false;
            if (q.name != ps[(size_t) q.part].kname[q.knob] || q.key != ed.optKey(ps[(size_t) q.part], q.knob)) return false;
        }
        return true;
    }
    void update()
    {
        if (! sameShape()) { rebuild(); return; }
        for (auto& c : cards) for (auto& q : c.ps) {
            if (&q == dragP) continue;
            q.val = ed.parts.parts[(size_t) q.part].kval[q.knob];
            refreshParam(q);
        }
        repaint();
    }
    // ---- drawing
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        for (int ci = 0; ci < (int) cards.size(); ++ci) {
            auto& c = cards[(size_t) ci];
            ui::panel(g, c.r, c.title);
            if (c.live) {                        // (the part the FM-1 shows)
                g.setColour(sk.accent.withAlpha(0.8f));
                g.drawRect(c.r, 1.5f);
            }
            for (int pi = 0; pi < (int) c.ps.size(); ++pi) drawParam(g, c.ps[(size_t) pi], hoverCard == ci && hoverParam == pi);
        }
        if (cards.empty()) {
            g.setColour(sk.dim);
            g.setFont(ui::label(14.0f));
            g.drawText("RIEN A REGLER ICI", getLocalBounds(), juce::Justification::centred);
        }
    }
    juce::String optAt(const Param& q, int i) const
    {
        auto it = ed.opts.find(q.key);
        return it != ed.opts.end() && i >= 0 && i < it->second.v.size() ? it->second.v[i] : juce::String();
    }
    void drawParam(juce::Graphics& g, const Param& q, bool hover)
    {
        const auto& sk = s();
        g.setColour(hover ? sk.ink : sk.dim);
        ui::fitText(g, q.name, q.row.withWidth(q.ctl.getX() - q.row.getX() - 6.0f), ui::label(12.5f), juce::Justification::centredLeft);
        auto r = q.ctl;
        switch (q.kind) {
        case menus::Toggle:
        case menus::Segments: {
            const float w = r.getWidth() / q.n;
            for (int i = 0; i < q.n; ++i)
                ui::button(g, { r.getX() + i * w + (i ? 1.0f : 0.0f), r.getY() + 4.0f, w - 2.0f, r.getHeight() - 8.0f },
                           optAt(q, i), i == q.cur, false, 11.5f);
            break;
        }
        case menus::Slider: {
            auto tr = r.reduced(0.0f, 5.0f);
            g.setColour(sk.panel2);
            g.fillRect(tr);
            const float t = q.n > 1 ? (float) q.cur / (q.n - 1) : 0.0f;
            g.setColour(sk.accent.withAlpha(hover || dragP == &q ? 0.55f : 0.38f));
            g.fillRect(tr.withWidth(juce::jmax(2.0f, tr.getWidth() * t)));
            if (q.n <= 33)                       // (few steps: each one marked)
                for (int i = 1; i < q.n - 1; ++i) {
                    g.setColour(sk.line.withAlpha(0.8f));
                    g.fillRect(tr.getX() + tr.getWidth() * i / (q.n - 1), tr.getBottom() - 5.0f, 1.0f, 5.0f);
                }
            g.setColour(sk.hot);
            g.fillRect(tr.getX() + tr.getWidth() * t - 1.0f, tr.getY(), 3.0f, tr.getHeight());
            g.setColour(sk.line);
            g.drawRect(tr, 1.0f);
            g.setColour(sk.ink);
            ui::fitText(g, q.val, tr.reduced(8.0f, 0.0f), ui::value(19.0f), juce::Justification::centredRight);
            break;
        }
        case menus::List: {
            auto b = r.reduced(0.0f, 4.0f);
            g.setColour(hover ? sk.panel2.brighter(0.08f) : sk.panel2);
            g.fillRect(b);
            g.setColour(sk.line);
            g.drawRect(b, 1.0f);
            g.setColour(sk.ink);
            ui::fitText(g, q.val, b.reduced(10.0f, 0.0f).withTrimmedRight(18.0f), ui::value(19.0f), juce::Justification::centredLeft);
            juce::Path tri;
            const float cx = b.getRight() - 14.0f, cy = b.getCentreY();
            tri.addTriangle(cx - 5.0f, cy - 3.0f, cx + 5.0f, cy - 3.0f, cx, cy + 4.0f);
            g.setColour(sk.accent);
            g.fillPath(tri);
            break;
        }
        default: {                                // (not looked at yet, or one value): its value, < > if it moves
            auto b = r.reduced(0.0f, 4.0f);
            if (q.kind == menus::Unknown) {
                ui::button(g, b.withWidth(26.0f), "<", false, false, 13.0f);
                ui::button(g, b.withLeft(b.getRight() - 26.0f), ">", false, false, 13.0f);
                b = b.reduced(30.0f, 0.0f);
            }
            g.setColour(sk.ink);
            ui::fitText(g, q.val, b, ui::value(19.0f));
        }
        }
    }
    // ---- the mouse
    std::pair<int, int> hit(juce::Point<float> p) const
    {
        for (int ci = 0; ci < (int) cards.size(); ++ci)
            for (int pi = 0; pi < (int) cards[(size_t) ci].ps.size(); ++pi)
                if (cards[(size_t) ci].ps[(size_t) pi].row.contains(p)) return { ci, pi };
        return { -1, -1 };
    }
    Param* at(std::pair<int, int> h) { return h.first < 0 ? nullptr : &cards[(size_t) h.first].ps[(size_t) h.second]; }
    void mouseMove(const juce::MouseEvent& e) override
    {
        auto h = hit(e.position);
        if (h.first != hoverCard || h.second != hoverParam) { hoverCard = h.first; hoverParam = h.second; repaint(); }
        auto* q = at(h);
        setMouseCursor(q && q->kind == menus::Slider && q->ctl.contains(e.position) ? juce::MouseCursor::LeftRightResizeCursor
                                                                                     : juce::MouseCursor::NormalCursor);
    }
    void mouseExit(const juce::MouseEvent&) override { hoverCard = hoverParam = -1; repaint(); }
    // to value i: the steps from where it is (a list that goes round: the short way)
    void go(Param& q, int i)
    {
        if (q.kind == menus::Unknown) return;
        i = q.wrap ? (i % q.n + q.n) % q.n : juce::jlimit(0, q.n - 1, i);
        int d = i - q.cur;
        if (q.wrap && std::abs(d) > q.n / 2) d += d > 0 ? -q.n : q.n;
        if (d == 0) return;
        ed.proc.setValue(q.part, q.knob, d);
        q.cur = i;
        q.val = optAt(q, i);
        repaint();
    }
    void nudge(Param& q, int d)                  // a step, as a detent: stops at the ends (a slider, buttons)
    {
        if (q.kind == menus::Unknown) { ed.proc.setValue(q.part, q.knob, d); return; }
        go(q, q.cur + d);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto* q = at(hit(e.position));
        if (! q || ! q->ctl.contains(e.position)) return;
        if (e.mods.isPopupMenu() && q->n > 1) { listMenu(*q); return; }
        switch (q->kind) {
        case menus::Toggle:
            if (q->n == 2) { go(*q, (int) ((e.position.x - q->ctl.getX()) / (q->ctl.getWidth() / 2))); }
            break;
        case menus::Segments: go(*q, (int) ((e.position.x - q->ctl.getX()) / (q->ctl.getWidth() / q->n))); break;
        case menus::List: listMenu(*q); break;
        case menus::Slider:
            dragP = q;
            dragFrom = dragSent = q->cur;
            // the steps' width: few steps wide, many narrow (the whole range over about the slider and a half)
            pxStep = juce::jlimit(2.5f, 28.0f, q->ctl.getWidth() * 1.4f / juce::jmax(1, q->n - 1));
            break;
        case menus::Unknown:
            nudge(*q, e.position.x < q->ctl.getCentreX() ? -1 : 1);
            break;
        default: break;
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! dragP) return;
        const float dx = (float) e.getDistanceFromDragStartX() - (float) e.getDistanceFromDragStartY() * 0.5f;
        const float slow = e.mods.isShiftDown() ? 4.0f : 1.0f;
        const int want = juce::jlimit(0, dragP->n - 1, dragFrom + (int) std::floor(dx / (pxStep * slow) + 0.5f));
        if (want != dragSent) { go(*dragP, want); dragSent = want; }
    }
    void mouseUp(const juce::MouseEvent&) override { dragP = nullptr; repaint(); }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        auto* q = at(hit(e.position));
        if (q && q->kind == menus::Slider && q->ctl.contains(e.position)) listMenu(*q);
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto* q = at(hit(e.position));
        if (! q || ! q->ctl.contains(e.position) || w.deltaY == 0.0f) { Component::mouseWheelMove(e, w); return; }
        nudge(*q, w.deltaY > 0 ? 1 : -1);
    }
    // every value in a menu (a list; a slider double-clicked or right-clicked): the one it has ticked
    void listMenu(Param& q)
    {
        auto it = ed.opts.find(q.key);
        if (it == ed.opts.end()) return;
        juce::PopupMenu pm;
        const auto& v = it->second.v;
        const int cols = v.size() > 40 ? (v.size() + 39) / 40 : 1;
        for (int i = 0; i < v.size(); ++i) {
            if (cols > 1 && i && i % ((v.size() + cols - 1) / cols) == 0) pm.addColumnBreak();
            pm.addItem(1 + i, v[i], true, i == q.cur);
        }
        const juce::String key = q.key;
        juce::Component::SafePointer<MenuContent> self(this);
        pm.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(q.ctl.toNearestInt()))
                             .withMinimumWidth((int) q.ctl.getWidth()).withStandardItemHeight(22),
                         [self, key](int r) {
                             if (! self || r <= 0) return;
                             for (auto& c : self->cards) for (auto& p : c.ps) if (p.key == key) { self->go(p, r - 1); return; }
                         });
    }
};

struct MenuPage : Part
{
    explicit MenuPage(LBE& e) : Part(e), content(e)
    {
        view.setViewedComponent(&content, false);
        view.setScrollBarsShown(true, false);
        view.setScrollBarThickness(8);
        addAndMakeVisible(view);
    }
    juce::Viewport view;
    MenuContent content;
    void resized() override
    {
        view.setBounds(getLocalBounds());
        content.availW = getWidth() - 12;
        content.rebuild();
    }
};

// ---------------------------------------------------------------------------------------------- the sequencer ---
// (FLEXOR, 24 Nov 2026: "il me faut un vrai séquenceur"): the pattern whole, as the firmware's SEQ has it (lb_seq):
// the 64 patterns above; the break's 32 steps (each its slice's own waveform), the kick's 64 and the drums' rows; a
// step clicked on or off, dragged to paint; its settings, its bar's, the lane's below. Every edit through SEQ's own
// code (lb_step), its undo too
struct SeqPanel : Part
{
    explicit SeqPanel(LBE& e) : Part(e) {}
    int selRow = 1, selCol = 0;          // 0 the break, 1..4 KICK SN HH CP
    int paintRow = -1, paintTo = 0, brkDragFrom = 0, brkSent = 0;
    std::set<int> painted;
    juce::Point<float> downAt;
    static constexpr float HEAD_W = 186.0f, BANK_H = 34.0f, RULER_H = 18.0f;
    static const char* laneName(int r) { static const char* N[5] = { "BREAK", "KICK", "SNARE", "HAT", "CLAP" }; return N[r]; }
    juce::Colour laneCol(int r) const
    {
        const auto& sk = s();
        switch (r) { case 0: return sk.ink; case 1: return sk.hot; case 2: return sk.accent; case 3: return sk.ink.withAlpha(0.75f); default: return sk.accent.brighter(0.3f); }
    }
    juce::Rectangle<float> gridArea() const { return { HEAD_W + 8.0f, BANK_H + RULER_H + 8.0f, getWidth() - HEAD_W - 16.0f, rowsH() }; }
    float rowH(int r) const { return r == 0 ? 84.0f : r == 1 ? 60.0f : 52.0f; }
    float rowsH() const { float h = 0; for (int r = 0; r < 5; ++r) h += rowH(r) + 4.0f; return h; }
    float rowY(int r) const { float y = gridArea().getY(); for (int i = 0; i < r; ++i) y += rowH(i) + 4.0f; return y; }
    float colW() const { return gridArea().getWidth() / 64.0f; }
    juce::Rectangle<float> cell(int r, int i) const { return { gridArea().getX() + i * colW() + 1.0f, rowY(r), colW() - 2.0f, rowH(r) }; }
    int laneLen(int r) const { return r == 0 ? 32 : 16 * juce::jmax(1, ed.seq.bars[r - 1]); }
    bool on(int r, int i) const { return r > 0 && (ed.seq.mask[r - 1][(i >> 4) & 3] >> (i & 15) & 1); }
    uint8_t sb(int r, int i) const { return r == 1 ? ed.seq.sb[0][i & 63] : ed.seq.sb[r - 1][i & 31]; }
    // the bank: 64 buttons, 4 groups; the tools at the right
    juce::Rectangle<float> patBtn(int n) const { return { 92.0f + n * 16.4f + (n / 16) * 10.0f, 6.0f, 15.0f, BANK_H - 10.0f }; }
    juce::Rectangle<float> tool(int i) const { return { (float) getWidth() - 8.0f - (4 - i) * 86.0f, 5.0f, 82.0f, BANK_H - 8.0f }; }
    juce::Rectangle<float> insp() const { const float y = gridArea().getBottom() + 6.0f; return { 8.0f, y, getWidth() - 16.0f, getHeight() - y - 4.0f }; }

    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        const auto& q = ed.seq;
        ui::panel(g, getLocalBounds().toFloat().reduced(1.0f));
        // bank
        g.setColour(sk.dim);
        g.setFont(ui::label(12.0f));
        g.drawText("PATTERNS", 12, 6, 76, (int) BANK_H - 10, juce::Justification::centredLeft);
        for (int n = 0; n < 64; ++n) {
            auto r = patBtn(n);
            const uint8_t* p = ed.pats + n * 40;
            bool any = false;
            for (int k = 4; k < 36; ++k) any |= p[k] != 0;
            const bool cur = n == q.pat, nxt = n == q.next;
            g.setColour(cur ? sk.hot : any ? sk.panel2.brighter(0.18f) : sk.panel2);
            g.fillRect(r);
            g.setColour(nxt ? sk.accent : sk.line);
            g.drawRect(r, nxt ? 2.0f : 1.0f);
            g.setColour(cur ? sk.bg : any ? sk.ink : sk.dim);
            g.setFont(ui::value(11.0f));
            g.drawText(juce::String(n + 1), r, juce::Justification::centred);
        }
        static const char* TOOLS[4] = { "COPIER", "COLLER", "ANNULER", "VIDER" };
        for (int i = 0; i < 4; ++i) ui::button(g, tool(i), i == 1 && q.clip >= 0 ? "COLLER P" + juce::String(q.clip + 1) : juce::String(TOOLS[i]), false, false, 11.5f);
        // ruler
        const auto ga = gridArea();
        for (int i = 0; i < 64; ++i) {
            if (i % 4) continue;
            g.setColour(i % 16 ? sk.dim : sk.ink);
            g.setFont(ui::label(i % 16 ? 10.0f : 11.0f));
            g.drawText(i % 16 ? juce::String(i + 1) : "MESURE " + juce::String(i / 16 + 1), (int) (ga.getX() + i * colW()) + 2, (int) (BANK_H + 4), 80, (int) RULER_H, juce::Justification::centredLeft);
        }
        for (int r = 0; r < 5; ++r) drawLane(g, r);
        // the bar lines
        for (int b = 1; b < 4; ++b) {
            g.setColour(sk.ink.withAlpha(0.35f));
            g.fillRect(ga.getX() + b * 16 * colW() - 1.0f, ga.getY(), 2.0f, ga.getHeight());
        }
        drawInspector(g);
    }
    void drawLane(juce::Graphics& g, int r)
    {
        const auto& sk = s();
        const auto& q = ed.seq;
        const float y = rowY(r), h = rowH(r);
        // its head
        juce::Rectangle<float> hd(10.0f, y, HEAD_W - 6.0f, h);
        g.setColour(r == selRow ? sk.panel2.brighter(0.06f) : sk.panel2);
        g.fillRect(hd);
        g.setColour(laneCol(r));
        g.fillRect(hd.withWidth(4.0f));
        g.setFont(ui::labelBold(15.0f));
        g.drawText(laneName(r), hd.reduced(12.0f, 4.0f).withHeight(18.0f), juce::Justification::centredLeft);
        g.setColour(sk.dim);
        if (r == 0) {
            ui::fitText(g, juce::String(q.brkname) + "  -  " + juce::String(q.slices) + " SLICES", hd.reduced(12.0f, 0).withTrimmedTop(24.0f).withHeight(16.0f), ui::label(11.0f), juce::Justification::centredLeft);
        } else {
            // its sound < name >, its length 16 32 64
            auto snd = soundBox(r);
            ui::button(g, snd.withWidth(20.0f), "<", false, false, 12.0f);
            ui::button(g, snd.withLeft(snd.getRight() - 20.0f), ">", false, false, 12.0f);
            g.setColour(sk.ink);
            ui::fitText(g, q.snd[r - 1], snd.reduced(22.0f, 0.0f), ui::value(15.0f));
            for (int i = 0; i < 3; ++i) ui::button(g, lenBox(r, i), juce::String(16 << i), laneLen(r) == (16 << i), false, 11.0f);
        }
        // its steps
        for (int i = 0; i < 64; ++i) {
            auto c = cell(r, i);
            const int len = laneLen(r);
            const bool echo = i >= len;
            const int src = i % len;
            const bool sel = r == selRow && i == selCol;
            const bool now = q.playing && (r == 0 ? q.bstep == src && ! echo : q.bar64 >= 0 && q.bar64 % len == src && ! echo);
            if (r == 0) drawBreakStep(g, c, src, echo, sel, now);
            else drawDrumStep(g, r, c, src, echo, sel, now);
        }
    }
    juce::Rectangle<float> soundBox(int r) const { return { 22.0f, rowY(r) + 24.0f, HEAD_W - 30.0f, 20.0f }; }
    juce::Rectangle<float> lenBox(int r, int i) const { return { 104.0f + i * 26.0f, rowY(r) + 3.0f, 24.0f, 18.0f }; }
    void drawDrumStep(juce::Graphics& g, int r, juce::Rectangle<float> c, int i, bool echo, bool sel, bool now)
    {
        const auto& sk = s();
        const bool o = on(r, i);
        const uint8_t b = sb(r, i);
        const juce::Colour lc = laneCol(r);
        g.setColour(o ? lc.withMultipliedAlpha(echo ? 0.3f : ((b >> 2 & 7) && (b >> 2 & 7) < 6 ? 0.55f : 1.0f))
                      : (i % 4 == 0 ? sk.panel2.brighter(0.1f) : sk.panel2).withMultipliedAlpha(echo ? 0.4f : 1.0f));
        g.fillRect(c);
        if (o && ! echo) {
            const int hits = (b & 3) + 1;
            if (hits > 1)                         // HITS: its strokes
                for (int k = 0; k < hits; ++k) {
                    g.setColour(sk.bg.withAlpha(0.7f));
                    g.fillRect(c.getX() + 2.0f, c.getY() + 4.0f + k * 6.0f, c.getWidth() - 4.0f, 2.0f);
                }
            if (b >> 5) {                         // NUDGE: a mark moved right
                g.setColour(sk.bg);
                g.fillRect(c.getX() + c.getWidth() * (0.15f + 0.1f * (b >> 5)), c.getBottom() - 6.0f, 3.0f, 4.0f);
            }
            if (r == 1 && ed.seq.note[i]) {       // the kick's note
                g.setColour(sk.bg);
                g.setFont(ui::value(11.0f));
                g.drawText((ed.seq.note[i] > 0 ? "+" : "") + juce::String(ed.seq.note[i]), c.withTrimmedTop(c.getHeight() - 16.0f), juce::Justification::centred);
            }
        }
        if (now) { g.setColour(sk.hot); g.drawRect(c.expanded(1.0f), 2.0f); }
        if (sel) { g.setColour(sk.accent); g.drawRect(c.expanded(1.0f), 2.0f); }
    }
    void drawBreakStep(juce::Graphics& g, juce::Rectangle<float> c, int i, bool echo, bool sel, bool now)
    {
        const auto& sk = s();
        const uint8_t* b = ed.seq.brk[i & 31];
        g.setColour((i % 4 == 0 ? sk.panel2.brighter(0.1f) : sk.panel2).withMultipliedAlpha(echo ? 0.5f : 1.0f));
        g.fillRect(c);
        // its sound: the slice it plays (LOOP: the break where it is), as a waveform
        const auto& w = ed.wave;
        const int n = (int) w.cols.size() / 4;
        float a0 = -1.0f, a1 = -1.0f;
        if (b[0] == 1) { a0 = i / 32.0f; a1 = (i + 1) / 32.0f; }
        else if (b[0] >= 2 && ! w.slices.empty()) {
            const int s = juce::jlimit(0, (int) w.slices.size() - 1, b[0] - 2);
            a0 = w.slices[(size_t) s];
            a1 = s + 1 < (int) w.slices.size() ? w.slices[(size_t) s + 1] : 1.0f;
        }
        if (n > 0 && a0 >= 0.0f) {
            const float mid = c.getCentreY() + 6.0f, half = c.getHeight() * 0.33f;
            const bool rev = b[2] & 1;
            for (float x = 0; x < c.getWidth(); x += 2.0f) {
                float t = x / c.getWidth();
                if (rev) t = 1.0f - t;
                const int col = juce::jlimit(0, n - 1, (int) ((a0 + (a1 - a0) * t) * n));
                const float pk = w.cols[(size_t) (4 * col)];
                g.setColour((b[0] == 1 ? sk.dim : sk.ink).withMultipliedAlpha(echo ? 0.35f : 1.0f));
                g.fillRect(c.getX() + x, mid - pk * half, 1.5f, juce::jmax(1.0f, 2.0f * pk * half));
            }
        }
        g.setColour(b[0] == 0 ? sk.dim : b[0] == 1 ? sk.dim : sk.hot);
        g.setFont(ui::value(13.0f));
        g.drawText(b[0] == 0 ? "-" : b[0] == 1 ? "" : juce::String(b[0] - 1), c.withHeight(16.0f), juce::Justification::centred);
        juce::String tags;
        if ((int8_t) b[1]) tags << ((int8_t) b[1] > 0 ? "+" : "") << (int) (int8_t) b[1];
        if (b[2] >> 1 & 3) tags << " x" << ((b[2] >> 1 & 3) + 1);
        if (b[2] & 1) tags << " R";
        if (tags.isNotEmpty()) {
            g.setColour(sk.accent);
            ui::fitText(g, tags.trim(), c.withTrimmedTop(c.getHeight() - 14.0f), ui::label(9.5f));
        }
        if (now) { g.setColour(sk.hot); g.drawRect(c.expanded(1.0f), 2.0f); }
        if (sel) { g.setColour(sk.accent); g.drawRect(c.expanded(1.0f), 2.0f); }
    }
    // ---- the inspector: the step, its bar, the lane
    struct Ctl { juce::String name, val; int op, c; int n, cur; juce::StringArray seg; juce::Rectangle<float> r; };
    std::vector<Ctl> ctls;
    std::vector<std::pair<juce::Rectangle<float>, int>> btns;   // (the bar's and the lane's tools)
    void drawInspector(juce::Graphics& g)
    {
        const auto& sk = s();
        const auto& q = ed.seq;
        auto a = insp();
        ui::panel(g, a, selRow == 0 ? "PAS " + juce::String(selCol % 32 + 1) + "  -  BREAK" : "PAS " + juce::String(selCol + 1) + "  -  " + laneName(selRow));
        ctls.clear();
        btns.clear();
        auto r = a.reduced(14.0f, 0.0f).withTrimmedTop(16.0f).withTrimmedBottom(10.0f);
        auto add = [&](const juce::String& name, const juce::String& val, int op, int c, int n, int cur, juce::StringArray seg, float w) {
            Ctl k { name, val, op, c, n, cur, seg, r.removeFromLeft(w) };
            r.removeFromLeft(14.0f);
            ctls.push_back(k);
        };
        if (selRow == 0) {
            const uint8_t* b = q.brk[selCol & 31];
            juce::StringArray sl { "OFF", "LOOP" };
            for (int i = 1; i <= q.slices; ++i) sl.add(juce::String(i));
            add("SLICE", sl[juce::jmin((int) b[0], sl.size() - 1)], 4, 0, sl.size(), b[0], {}, 230.0f);
            add("PITCH", ((int8_t) b[1] > 0 ? "+" : "") + juce::String((int) (int8_t) b[1]) + " ST", 4, 1, 25, (int8_t) b[1] + 12, {}, 190.0f);
            add("REPEAT", {}, 4, 2, 4, b[2] >> 1 & 3, { "X1", "X2", "X3", "X4" }, 160.0f);
            add("SENS", {}, 4, 3, 2, b[2] & 1, { "AVANT", "ARRIERE" }, 150.0f);
        } else {
            const uint8_t b = sb(selRow, selCol);
            if (selRow == 1) add("NOTE", (q.note[selCol] > 0 ? "+" : "") + juce::String(q.note[selCol]) + " ST", 2, 0, 16, q.note[selCol] + 8, {}, 150.0f);
            add("HITS", {}, 2, 1, 4, b & 3, { "1", "2", "3", "4" }, 130.0f);
            add("CHANCE", {}, 2, 2, 6, juce::jmin(5, b >> 2 & 7), { "100%", "75%", "50%", "25%", "1:2", "2:2" }, 260.0f);
            add("NUDGE", (b >> 5) ? "+" + juce::String(b >> 5) + "/16" : "PILE", 2, 3, 8, b >> 5, {}, 140.0f);
        }
        for (auto& k : ctls) {
            g.setColour(sk.dim);
            g.setFont(ui::label(11.5f));
            g.drawText(k.name, k.r.withHeight(16.0f), juce::Justification::centredLeft);
            auto c = k.r.withTrimmedTop(18.0f).withHeight(30.0f);
            if (k.seg.size()) {
                const float w = c.getWidth() / k.seg.size();
                for (int i = 0; i < k.seg.size(); ++i) ui::button(g, { c.getX() + i * w, c.getY(), w - 2.0f, c.getHeight() }, k.seg[i], i == k.cur, false, 11.5f);
            } else {
                g.setColour(sk.panel2);
                g.fillRect(c);
                const float t = k.n > 1 ? (float) k.cur / (k.n - 1) : 0.0f;
                g.setColour(sk.accent.withAlpha(0.4f));
                g.fillRect(c.withWidth(juce::jmax(2.0f, c.getWidth() * t)));
                g.setColour(sk.hot);
                g.fillRect(c.getX() + c.getWidth() * t - 1.0f, c.getY(), 3.0f, c.getHeight());
                g.setColour(sk.line);
                g.drawRect(c, 1.0f);
                g.setColour(sk.ink);
                ui::fitText(g, k.val, c.reduced(8.0f, 0.0f), ui::value(18.0f), juce::Justification::centredRight);
            }
        }
        // the tools: the bar (FILL - +, CLEAR, 4X4, its copy onto bar n), the lane (a step earlier, later), the pattern
        auto t = r.withTrimmedTop(18.0f).withHeight(30.0f);
        auto bt = [&](const juce::String& s2, int id, float w) { auto b = t.removeFromLeft(w); t.removeFromLeft(6.0f); ui::button(g, b, s2, false, false, 11.0f); btns.push_back({ b, id }); };
        g.setColour(sk.dim);
        g.setFont(ui::label(11.5f));
        g.drawText(selRow ? "MESURE " + juce::String(selCol / 16 + 1) + "  (COPIER VERS > M..)  /  LIGNE" : "LIGNE", r.withHeight(16.0f), juce::Justification::centredLeft);
        if (selRow) {
            bt("FILL -", 1, 50.0f); bt("FILL +", 2, 50.0f); bt("VIDER", 3, 52.0f); bt("4X4", 4, 40.0f);
            for (int k = 0; k < laneLen(selRow) / 16; ++k) if (k != selCol / 16) bt("> M" + juce::String(k + 1), 10 + k, 46.0f);
        } else {
            bt((selCol & 31) < 16 ? "COPIER > 17-32" : "COPIER > 1-16", 11, 110.0f);
            bt("VIDER BREAK", 6, 96.0f);
        }
        bt("<< 1 PAS", 7, 66.0f);
        bt("1 PAS >>", 8, 66.0f);
    }
    // ---- the mouse
    int rowAt(float y) const { for (int r = 0; r < 5; ++r) if (y >= rowY(r) && y < rowY(r) + rowH(r)) return r; return -1; }
    int colAt(float x) const { const auto ga = gridArea(); return x < ga.getX() || x >= ga.getRight() ? -1 : (int) ((x - ga.getX()) / colW()); }
    void stepOp(int op, int a, int b, int c, int delta)
    {
        // (the break's SLICE and PITCH take the turn whole; the others move one a call)
        if (op == 4 && c <= 1) { ed.proc.step(op, a, b, c, delta); return; }
        for (int i = 0; i < std::abs(delta); ++i) ed.proc.step(op, a, b, c, delta > 0 ? 1 : -1);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        downAt = p;
        // bank, tools
        for (int n = 0; n < 64; ++n) if (patBtn(n).contains(p)) { ed.proc.step(5, n, 0, 0, 0); return; }
        for (int i = 0; i < 4; ++i)
            if (tool(i).contains(p)) {
                if (i == 0) ed.proc.step(7, 0, 0, 0, 0);
                else if (i == 1) ed.proc.step(8, 0, 0, 0, 0);
                else if (i == 2) ed.proc.step(6, 0, 0, 0, 0);
                else {
                    juce::PopupMenu m;
                    m.addItem(1, "Vider les drums (le break garde ses pas)");
                    m.addItem(2, "Vider les pas du break");
                    m.addItem(3, "Vider tout le pattern");
                    juce::Component::SafePointer<SeqPanel> self(this);
                    m.showMenuAsync(juce::PopupMenu::Options(), [self](int r) { if (self && r) self->ed.proc.step(r == 1 ? 11 : r == 2 ? 12 : 13, 0, 0, 0, 0); });
                }
                return;
            }
        // lane heads: the sound, the length
        for (int r = 1; r < 5; ++r) {
            auto snd = soundBox(r);
            if (snd.withWidth(20.0f).contains(p)) { ed.proc.step(3, r - 1, 0, 1, -1); return; }
            if (snd.withLeft(snd.getRight() - 20.0f).contains(p)) { ed.proc.step(3, r - 1, 0, 1, 1); return; }
            for (int i = 0; i < 3; ++i)
                if (lenBox(r, i).contains(p)) {
                    const int now = laneLen(r) == 16 ? 0 : laneLen(r) == 32 ? 1 : 2;
                    stepOp(3, r - 1, 0, 3, i - now);
                    return;
                }
        }
        // the inspector
        for (auto& k : ctls) {
            auto c = k.r.withTrimmedTop(18.0f).withHeight(30.0f);
            if (! c.contains(p)) continue;
            int want;
            if (k.seg.size()) want = (int) ((p.x - c.getX()) / (c.getWidth() / k.seg.size()));
            else { want = k.cur; sliderCtl = &k - ctls.data(); sliderFrom = k.cur; sliderSent = k.cur; return; }
            ctlTo(k, want);
            return;
        }
        for (auto& b : btns)
            if (b.first.contains(p)) { tool2(b.second); return; }
        // the grid
        const int r = rowAt(p.y), i = colAt(p.x);
        if (r < 0 || i < 0 || i >= laneLen(r)) return;
        selRow = r;
        selCol = i;
        if (r > 0 && ! e.mods.isPopupMenu()) {
            paintRow = r;
            paintTo = on(r, i) ? 0 : 1;
            painted = { i };
            ed.proc.step(1, r - 1, i, 0, 0);
            ed.seq.mask[r - 1][i >> 4] ^= (uint16_t) (1u << (i & 15));
        } else if (r == 0) { brkDragFrom = brkSent = ed.seq.brk[i & 31][0]; }
        repaint();
    }
    int sliderCtl = -1, sliderFrom = 0, sliderSent = 0;
    void ctlTo(const Ctl& k, int want)
    {
        want = juce::jlimit(0, k.n - 1, want);
        if (want == k.cur) return;
        if (selRow == 0) stepOp(4, 0, selCol & 31, k.c, want - k.cur);
        else stepOp(2, selRow - 1, selCol, k.c, want - k.cur);
    }
    void tool2(int id)
    {
        const int d = selRow - 1, bar = selCol / 16;
        if (id == 1 || id == 2) ed.proc.step(3, d, bar, 0, id == 1 ? -1 : 1);
        else if (id == 3 || id == 4) ed.proc.step(3, d, bar, 2, id == 3 ? -1 : 1);
        else if (id == 6) ed.proc.step(12, 0, 0, 0, 0);
        else if (id == 7 || id == 8) ed.proc.step(9, selRow == 0 ? 4 : d, 0, 0, id == 7 ? -1 : 1);
        else if (id >= 10 && selRow == 0) ed.proc.step(10, 4, (selCol & 31) / 16, 1 - (selCol & 31) / 16, 0);   // (the break: one half onto the other)
        else if (id >= 10) ed.proc.step(10, d, bar, id - 10, 0);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (sliderCtl >= 0 && sliderCtl < (int) ctls.size()) {
            auto& k = ctls[(size_t) sliderCtl];
            const float px = juce::jlimit(3.0f, 24.0f, k.r.getWidth() * 1.3f / juce::jmax(1, k.n - 1));
            const int want = juce::jlimit(0, k.n - 1, sliderFrom + (int) std::floor(e.getDistanceFromDragStartX() / px + 0.5f));
            if (want != sliderSent) {
                Ctl k2 = k;
                k2.cur = sliderSent;
                ctlTo(k2, want);
                sliderSent = want;
            }
            return;
        }
        if (paintRow > 0) {                      // painting: each step met set as the first became
            const int i = colAt(e.position.x);
            if (i >= 0 && i < laneLen(paintRow) && ! painted.count(i)) {
                painted.insert(i);
                if ((on(paintRow, i) ? 1 : 0) != paintTo) {
                    ed.proc.step(1, paintRow - 1, i, 0, 0);
                    ed.seq.mask[paintRow - 1][i >> 4] ^= (uint16_t) (1u << (i & 15));
                }
                repaint();
            }
            return;
        }
        if (selRow == 0 && rowAt(downAt.y) == 0) {   // a break step dragged up / down: its slice
            const int want = juce::jlimit(0, ed.seq.slices + 1, brkDragFrom + (int) (-e.getDistanceFromDragStartY() / 8));
            if (want != brkSent) { ed.proc.step(4, 0, selCol & 31, 0, want - brkSent); brkSent = want; }
        }
    }
    void mouseUp(const juce::MouseEvent&) override { paintRow = -1; sliderCtl = -1; painted.clear(); }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int r = rowAt(e.position.y), i = colAt(e.position.x);
        const int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (! d || r < 0 || i < 0 || i >= laneLen(r)) return;
        selRow = r;
        selCol = i;
        if (r == 0) ed.proc.step(4, 0, i & 31, 0, d);      // the break: its slice
        else if (on(r, i)) ed.proc.step(2, r - 1, i, 1, d);   // a drum on: its HITS
        repaint();
    }
};

// ------------------------------------------------------------------------- the FM-1's screen: a window aside ---
// (FLEXOR: "garder l'aperçu dans un coin et régler sa taille, mais qu'il ne soit pas quelque chose que je dois
// constamment regarder"): floating, moved by its bar, sized by its corner, kept where it was left
struct Machine : Part
{
    explicit Machine(LBE& e) : Part(e), corner(this, &limits)
    {
        limits.setSizeLimits(140, 162, 600, 622);
        limits.setFixedAspectRatio(240.0 / 262.0);
        addAndMakeVisible(corner);
    }
    juce::Image screen { juce::Image::RGB, 240, 240, true };
    uint32_t serial = 0;
    std::vector<uint16_t> px = std::vector<uint16_t>(240 * 240);
    juce::ComponentBoundsConstrainer limits;
    juce::ResizableCornerComponent corner;
    juce::ComponentDragger dragger;
    void update()
    {
        if (! ed.proc.screenIfNew(serial, px.data())) return;
        juce::Image::BitmapData bd(screen, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 240; ++y)
            for (int x = 0; x < 240; ++x) {
                const uint16_t v = px[(size_t) (y * 240 + x)];
                bd.setPixelColour(x, y, juce::Colour((uint8_t) ((v >> 11) << 3), (uint8_t) (((v >> 5) & 63) << 2), (uint8_t) ((v & 31) << 3)));
            }
        repaint();
    }
    void resized() override { corner.setBounds(getWidth() - 14, getHeight() - 14, 14, 14); }
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        g.setColour(sk.bg);
        g.fillRect(getLocalBounds());
        g.setColour(sk.line);
        g.drawRect(getLocalBounds(), 1);
        g.setColour(sk.panel2);
        g.fillRect(1, 1, getWidth() - 2, 20);
        g.setColour(sk.dim);
        g.setFont(ui::label(11.0f));
        g.drawText("APERCU FM-1", 6, 1, getWidth() - 30, 20, juce::Justification::centredLeft);
        g.drawText("x", getWidth() - 20, 1, 16, 20, juce::Justification::centred);
        auto r = getLocalBounds().toFloat().withTrimmedTop(22.0f).reduced(1.0f);
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImage(screen, r, juce::RectanglePlacement::centred);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.y < 22 && e.x > getWidth() - 22) { ed.toggleMachine(); return; }
        dragger.startDraggingComponent(this, e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        dragger.dragComponent(this, e, nullptr);
        if (auto* p = getParentComponent())
            setTopLeftPosition(juce::jlimit(0, p->getWidth() - getWidth(), getX()), juce::jlimit(0, p->getHeight() - getHeight(), getY()));
    }
    void mouseUp(const juce::MouseEvent&) override { keep(); }
    void keep() { ui::settings().getUserSettings()->setValue("machineBox", getBounds().toString()); }
    void moved() override {}
    void parentSizeChanged() override {}
};

// ---------------------------------------------------------------------------------------------------- pads ---
struct Pads : Part
{
    explicit Pads(LBE& e) : Part(e) {}
    int held = -1;                       // 0..15 white, 100.. black
    juce::Rectangle<float> white(int i) const { const float w = (W - 40.0f) / 16.0f; return { 20.0f + i * w, 108.0f, w - 6.0f, 70.0f }; }
    juce::Rectangle<float> black(int i) const { const float w = (W - 40.0f) / 12.0f; return { 20.0f + i * w, 24.0f, w - 6.0f, 64.0f }; }
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        ui::panel(g, getLocalBounds().toFloat().reduced(1.0f), "PADS  /  TOUCHES");
        auto& km = KeyMap::get();
        for (int i = 0; i < 12; ++i) {
            auto r = black(i);
            const bool on = ed.m.padon[i] || held == 100 + i;
            g.setColour(on ? sk.accent : sk.panel2);
            g.fillRect(r);
            g.setColour(on ? sk.hot : sk.line);
            g.drawRect(r, 1.0f);
            g.setColour(on ? sk.bg : sk.ink);
            ui::fitText(g, ed.m.pad[i], r.withHeight(34).reduced(4, 0), ui::labelBold(16.0f));
            g.setColour(on ? sk.bg.withAlpha(0.8f) : sk.dim);
            ui::fitText(g, ed.m.padlong[i], r.withTrimmedTop(32).withHeight(14).reduced(4, 0), ui::label(10.0f));
            const auto k = i < 11 ? km.labelOf(KeyAction::Black, i) : km.labelOf(KeyAction::Button, LB_REC);
            g.setFont(ui::value(18.0f));
            g.drawText(k, r.withTrimmedTop(46).reduced(5, 0), juce::Justification::centredRight);
        }
        for (int i = 0; i < 16; ++i) {
            auto r = white(i);
            const bool on = ed.m.key == i || held == i || (ed.m.slice == i && ed.m.playing);
            g.setColour(on ? sk.hot : sk.panel2.brighter(0.05f));
            g.fillRect(r);
            g.setColour(on ? sk.hot : sk.line.brighter(0.2f));
            g.drawRect(r, 1.0f);
            g.setColour(on ? sk.bg : sk.ink);
            g.setFont(ui::value(34.0f));
            g.drawText(juce::String(i + 1), r.reduced(6, 2), juce::Justification::topLeft);
            g.setColour(on ? sk.bg : sk.dim);
            g.setFont(ui::value(18.0f));
            g.drawText(km.labelOf(KeyAction::White, i), r.reduced(6, 4), juce::Justification::bottomRight);
        }
    }
    int hit(juce::Point<float> p) const
    {
        for (int i = 0; i < 12; ++i) if (black(i).contains(p)) return 100 + i;
        for (int i = 0; i < 16; ++i) if (white(i).contains(p)) return i;
        return -1;
    }
    void press(int h, bool d)
    {
        if (h < 0) return;
        if (h >= 100) { if (h == 111) ed.hold(LB_REC, d); else ed.key(true, h - 100, d); }
        else ed.key(false, h, d);
    }
    void mouseDown(const juce::MouseEvent& e) override { held = hit(e.position); press(held, true); repaint(); }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const int h = hit(e.position);
        if (h != held) { press(held, false); held = h; press(held, true); repaint(); }
    }
    void mouseUp(const juce::MouseEvent&) override { press(held, false); held = -1; repaint(); }
};

// ---------------------------------------------------------------------------------------------------- root ---
struct LoafBreakerEditor::Root : Part
{
    explicit Root(LBE& e) : Part(e), header(e), cats(e), tabs(e), wave(e), menus(e), seqp(e), machine(e), pads(e)
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &header, &cats, &tabs, &wave, &menus, &seqp, &pads, &machine })
            addAndMakeVisible(c);
        setSize(W, H);
        auto box = juce::Rectangle<int>::fromString(ui::settings().getUserSettings()->getValue("machineBox"));
        if (box.getWidth() < 150 || ! juce::Rectangle<int>(0, 0, W, H).contains(box)) box = { W - 196, 466, 180, 196 };
        machine.setBounds(box);
    }
    Header header;
    Categories cats;
    Tabs tabs;
    WavePanel wave;
    MenuPage menus;
    SeqPanel seqp;
    Machine machine;
    Pads pads;
    bool seqShown = false;
    void resized() override
    {
        header.setBounds(0, 0, W, 86);
        cats.setBounds(0, 86, W, 44);
        tabs.setBounds(0, 130, W, 36);
        seqShown = ed.seqView();
        wave.setVisible(! seqShown);
        menus.setVisible(! seqShown);
        seqp.setVisible(seqShown);
        wave.setBounds(8, 172, W - 16, 104);
        menus.setBounds(8, 282, W - 16, 404);
        seqp.setBounds(8, 172, W - 16, 514);
        machine.setVisible(ed.showMachine);
        machine.toFront(false);
        pads.setBounds(8, 690, W - 16, 202);
    }
    void paint(juce::Graphics& g) override { g.fillAll(s().bg); }
    void paintOverChildren(juce::Graphics& g) override
    {
        ui::grain(g, getLocalBounds(), 1.0f);
        ui::vignette(g, getLocalBounds().toFloat());
    }
};

// ------------------------------------------------------------------------------------------------ the editor ---
LoafBreakerEditor::LoafBreakerEditor(LoafBreakerProcessor& p) : AudioProcessorEditor(&p), proc(p)
{
    showMachine = ui::settings().getUserSettings()->getBoolValue("machine", true);
    root = std::make_unique<Root>(*this);
    addAndMakeVisible(*root);
    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(960, 600, 2880, 1800);
    getConstrainer()->setFixedAspectRatio((double) W / H);
    setSize(1200, 750);
    startTimerHz(30);
}

LoafBreakerEditor::~LoafBreakerEditor() { stopTimer(); }

void LoafBreakerEditor::resized()
{
    const float s = (float) getWidth() / W;
    root->setTransform(juce::AffineTransform::scale(s));
    root->setBounds(0, 0, W, H);
    root->resized();
}

void LoafBreakerEditor::paint(juce::Graphics& g) { g.fillAll(ui::cur().bg); }

void LoafBreakerEditor::timerCallback()
{
    proc.idlePump();                     // (no audio running: the firmware runs on all the same)
    if (proc.modelIfNew(modelSerial, m)) {
        root->header.repaint();
        root->cats.repaint();
        root->tabs.repaint();
        root->pads.repaint();
        root->wave.repaint();
        if (juce::String(m.dom) != lastDom) { lastDom = m.dom; opts.clear(); asked.clear(); }   // (another domain: its lists looked at again)
        if (seqView() != root->seqShown) root->resized();
        // a source asked for (SOUNDS: KICK, DRUMS, BREAK): SEL + EDIT until it is the one
        if (pendingSource.isNotEmpty()) {
            if (juce::String(m.label) == pendingSource || ++sourceTries > 4) pendingSource.clear();
            else if (sourceTries % 2 == 0) { hold(LB_SEL, true); tap(LB_EDIT); juce::Timer::callAfterDelay(120, [this] { hold(LB_SEL, false); }); }
        }
    }
    if (proc.waveIfNew(waveSerial, wave)) {
        root->wave.repaint();
        root->seqp.repaint();
    }
    if (proc.partsIfNew(partsSerial, parts))
        root->menus.content.update();
    // the answers awaited: each setting's values
    for (auto it = asked.begin(); it != asked.end();) {
        lb_opts_t* o = optBuf.get();
        if (! proc.takeOptions(it->first, *o)) { ++it; continue; }
        if (o->n > 0) {
            Opts v;
            for (int i = 0; i < o->n; ++i) v.v.add(juce::String(o->v[i]));
            v.wrap = o->wrap != 0;
            v.more = o->more != 0;
            opts[it->second] = v;
        }
        it = asked.erase(it);
        root->menus.content.update();
    }
    if (seqView()) {
        if (proc.seqIfNew(seqSerial, seq)) root->seqp.repaint();
        if (proc.patternsIfNew(patsSerial, pats)) root->seqp.repaint();
    }
    float l, r;
    proc.meters(l, r);
    root->header.meters(l, r);
    if (showMachine) root->machine.update();
}

juce::String LoafBreakerEditor::pageKey() const
{
    return juce::String(m.dom) + "|" + m.label + "|" + (m.tab_on >= 0 && m.tab_on < m.ntabs ? juce::String(m.tabs[m.tab_on]) : juce::String());
}
juce::String LoafBreakerEditor::optKey(const lb_part_t& p, int knob) const { return pageKey() + "|" + p.part + "|" + p.kname[knob & 3]; }
void LoafBreakerEditor::wantOptions(int part, int knob)
{
    if (part < 0 || part >= (int) parts.parts.size()) return;
    const auto& p = parts.parts[(size_t) part];
    const auto key = optKey(p, knob);
    for (auto& a : asked) if (a.second == key) return;
    if (asked.size() >= 12) return;
    const int id = proc.askOptions(part, knob, p.part, p.kname[knob & 3]);
    if (id > 0) asked[id] = key;
}
bool LoafBreakerEditor::seqView() const
{
    if (juce::String(m.dom) != "PATTERN") return false;
    const juce::String t = m.tab_on >= 0 && m.tab_on < m.ntabs ? juce::String(m.tabs[m.tab_on]) : juce::String();
    return t != "GROOVE" && t != "SETTINGS";
}
void LoafBreakerEditor::toggleMachine()
{
    showMachine = ! showMachine;
    ui::settings().getUserSettings()->setValue("machine", showMachine);
    root->resized();
    root->repaint();
}

void LoafBreakerEditor::tap(int b)
{
    proc.send(LoafBreakerProcessor::CmdButton, b, 1);
    juce::Component::SafePointer<LoafBreakerEditor> self(this);
    juce::Timer::callAfterDelay(60, [self, b] { if (self) self->proc.send(LoafBreakerProcessor::CmdButton, b, 0); });
}
void LoafBreakerEditor::hold(int b, bool d) { proc.send(LoafBreakerProcessor::CmdButton, b, d ? 1 : 0); }
void LoafBreakerEditor::turn(int e, int n) { if (n) proc.send(LoafBreakerProcessor::CmdTurn, e, n); }
void LoafBreakerEditor::key(bool black, int i, bool d)
{
    const int k = proc.keyIndex(black, i);
    if (k >= 0) proc.send(LoafBreakerProcessor::CmdKey, k, d ? 100 : 0);
}
void LoafBreakerEditor::goDomain(int d)
{
    if (juce::String(m.dom) != DOMS[d].name) tap(DOMS[d].button);
}
void LoafBreakerEditor::goSource(const juce::String& label)
{
    if (juce::String(m.dom) != "SOUNDS") tap(LB_EDIT);
    if (juce::String(m.label) == label) return;
    pendingSource = label;
    sourceTries = 0;
}
void LoafBreakerEditor::toast(const juce::String& t)
{
    toastText = t;
    toastUntil = juce::Time::getMillisecondCounterHiRes() + 6000.0;
    root->header.repaint();
}

void LoafBreakerEditor::menu()
{
    juce::PopupMenu skins;
    for (int i = 0; i < ui::skinCount(); ++i) skins.addItem(100 + i, ui::skin(i).name, true, i == ui::curIndex());
    juce::PopupMenu fm1;
    fm1.addItem(201, "Envoyer tout au FM-1");
    fm1.addItem(202, "Recuperer tout du FM-1");
    fm1.addItem(203, "Exporter une sauvegarde FM-1 (.bak)...");
    fm1.addItem(204, "Importer une sauvegarde FM-1 (.bak)...");
    juce::PopupMenu mm;
    mm.addSubMenu("Skin", skins);
    mm.addItem(1, "Clavier (touches)...");
    mm.addItem(2, "Apercu du FM-1 (fenetre flottante)", true, showMachine);
    mm.addSeparator();
    mm.addSubMenu("Machine FM-1", fm1);
    juce::Component::SafePointer<LoafBreakerEditor> self(this);
    mm.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&root->header), [self](int r) {
        if (! self || r == 0) return;
        if (r >= 100 && r < 200) { ui::setSkin(r - 100); self->root->repaint(); return; }
        if (r == 1) {
            juce::DialogWindow::LaunchOptions o;
            o.content.setOwned(new KeyMapEditor());
            o.dialogTitle = "LOAF BREAKER - CLAVIER";
            o.dialogBackgroundColour = ui::cur().bg;
            o.escapeKeyTriggersCloseButton = true;
            o.useNativeTitleBar = true;
            o.resizable = false;
            o.launchAsync();
            return;
        }
        if (r == 2) { self->toggleMachine(); return; }
        auto& p = self->proc;
        auto progress = [self](int pct, const juce::String& t) { if (self) self->toast(pct >= 0 && pct < 100 ? t + " " + juce::String(pct) + "%" : t); };
        if (r == 201 || r == 203) {
            juce::MemoryBlock img;
            juce::String rep;
            if (! p.exportFm1(img, rep)) { self->toast("EXPORT IMPOSSIBLE"); return; }
            if (r == 201) {
                p.link.send(img, progress, [self, rep](bool ok, const juce::String& t, const juce::MemoryBlock&) { if (self) self->toast(ok ? t + " " + rep : t); });
            } else {
                self->chooser = std::make_unique<juce::FileChooser>("Exporter pour le FM-1",
                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LOAF BREAKER.bak"), "*.bak");
                self->chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                    [self, img, rep](const juce::FileChooser& fc) {
                        auto f = fc.getResult();
                        if (self && f != juce::File()) self->toast(f.replaceWithData(img.getData(), img.getSize()) ? "EXPORTE: " + f.getFileName() : "ECRITURE IMPOSSIBLE");
                    });
            }
        } else if (r == 202) {
            p.link.fetch(progress, [self](bool ok, const juce::String& t, const juce::MemoryBlock& img) {
                if (! self) return;
                if (! ok) { self->toast(t); return; }
                juce::String rep;
                self->toast(self->proc.importFm1(img, rep) ? t : rep);
            });
        } else if (r == 204) {
            self->chooser = std::make_unique<juce::FileChooser>("Importer une sauvegarde FM-1",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.bak");
            self->chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [self](const juce::FileChooser& fc) {
                    auto f = fc.getResult();
                    juce::MemoryBlock img;
                    if (! self || ! f.loadFileAsData(img)) return;
                    juce::String rep;
                    self->toast(self->proc.importFm1(img, rep) ? "IMPORTE" : rep);
                });
        }
    });
}

// the computer's keyboard: KeyMap's actions (turns and buttons on a press; slices, pads and SEL held)
bool LoafBreakerEditor::keyPressed(const juce::KeyPress& k)
{
    int code = k.getKeyCode();
    if (code >= 'A' && code <= 'Z') code += 'a' - 'A';
    const int a = KeyMap::get().actionOf(code);
    if (a < 0) return false;
    const auto& act = KeyMap::get().actions()[(size_t) a];
    if (act.kind == KeyAction::Turn && act.index == KeyAction::Tempo) proc.send(LoafBreakerProcessor::CmdTempo, 0, act.dir);
    else if (act.kind == KeyAction::Turn) turn(act.index, act.dir);
    else if (act.kind == KeyAction::Button && act.index != LB_SEL && act.index != LB_REC && ! k.getModifiers().isAnyModifierKeyDown())
        tap(act.index);
    return true;
}

bool LoafBreakerEditor::keyStateChanged(bool)
{
    bool used = false;
    auto& km = KeyMap::get();
    for (size_t a = 0; a < km.actions().size(); ++a) {
        const auto& act = km.actions()[a];
        const bool holdable = act.kind == KeyAction::White || act.kind == KeyAction::Black ||
                              (act.kind == KeyAction::Button && (act.index == LB_SEL || act.index == LB_REC));
        const int code = km.keyOf((int) a);
        if (! holdable || ! code) continue;
        const bool down = juce::KeyPress::isKeyCurrentlyDown(code);
        const bool was = heldKeys.count((int) a) > 0;
        if (down == was) continue;
        if (down) heldKeys.insert((int) a); else heldKeys.erase((int) a);
        if (act.kind == KeyAction::Button) hold(act.index, down);
        else key(act.kind == KeyAction::Black, act.index, down);
        used = true;
    }
    if (used) root->pads.repaint();
    return used;
}

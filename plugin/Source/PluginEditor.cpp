#include "PluginEditor.h"
#include "Ui.h"
#include "Exchange.h"
#include <set>
#include <optional>
#include <tuple>
#include <array>

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
    int icon = 0;                        // (1: the window's maximise / restore square)
    void paint(juce::Graphics& g) override
    {
        ui::button(g, getLocalBounds().toFloat(), text, on || down, isMouseOver(), th);
        if (icon == 1) {
            const auto& sk = s();
            auto r = getLocalBounds().toFloat().withSizeKeepingCentre(16.0f, 14.0f);
            g.setColour(down ? sk.bg : isMouseOver() ? sk.hot : sk.ink);
            if (ed.screen == 3) {        // (maximised: two squares, the window to put back)
                g.drawRect(r.translated(3.0f, -3.0f).withSizeKeepingCentre(12.0f, 10.0f), 1.5f);
                g.fillRect(r.translated(-2.0f, 2.0f).withSizeKeepingCentre(12.0f, 10.0f).withHeight(3.0f));
                g.drawRect(r.translated(-2.0f, 2.0f).withSizeKeepingCentre(12.0f, 10.0f), 1.5f);
            } else {
                g.drawRect(r, 1.5f);
                g.fillRect(r.withHeight(3.5f));
            }
        }
    }
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
        // (FLEXOR: "un bouton de fenêtre cliquable pour mettre en plein écran avec bordure"): the window maximised, or back
        maxi = std::make_unique<PixButton>(e, "", [&e](bool d) { if (d) e.screenMode(e.screen == 3 ? 0 : 3); });
        maxi->icon = 1;
        addAndMakeVisible(*maxi);
        sync = std::make_unique<PixButton>(e, "SYNC", [&e](bool d) {
            if (! d) return;
            auto* p = e.proc.params.getParameter("sync");
            p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
        });
        addAndMakeVisible(*sync);
    }
    std::unique_ptr<PixButton> play, menu, sync, maxi;
    float vl = 0, vr = 0, hl = 0, hr = 0, tdrag = 0.0f;
    bool tempoHover = false, tempoDrag = false;
    // the tempo: dragged up / down or the wheel, 1 BPM a step (SELECT never touches it in the VST)
    static juce::Rectangle<float> tempoArea() { return { 500, 8, 136, 64 }; }
    // the break (FLEXOR: "cliquer sur le nom du break ouvre une liste déroulante ... next break et previous break en
    // dessous du nom du break"): its name a menu by category, < PLAYLIST > under it
    static juce::Rectangle<float> breakName() { return { 724, 22, 216, 36 }; }
    static juce::Rectangle<float> breakPrev() { return { 724, 60, 26, 20 }; }
    static juce::Rectangle<float> breakList() { return { 754, 60, 156, 20 }; }
    static juce::Rectangle<float> breakNext() { return { 914, 60, 26, 20 }; }
    int brkHover = 0;
    void tempo(int n) { if (n) ed.proc.send(LoafBreakerProcessor::CmdTempo, 0, n); }
    void mouseMove(const juce::MouseEvent& e) override
    {
        const int bh = breakName().contains(e.position) ? 1 : breakPrev().contains(e.position) ? 2 : breakList().contains(e.position) ? 3 : breakNext().contains(e.position) ? 4 : 0;
        if (bh != brkHover) { brkHover = bh; setMouseCursor(bh ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); repaint(); }
        const bool h = tempoArea().contains(e.position);
        if (h != tempoHover) { tempoHover = h; setMouseCursor(h ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor); repaint(); }
    }
    void mouseExit(const juce::MouseEvent&) override { if (tempoHover) { tempoHover = false; setMouseCursor(juce::MouseCursor::NormalCursor); repaint(); } }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (breakName().contains(e.position)) { ed.breakMenu(); return; }
        if (breakPrev().contains(e.position)) { ed.proc.breakStep(-1); return; }
        if (breakNext().contains(e.position)) { ed.proc.breakStep(1); return; }
        if (breakList().contains(e.position)) { ed.playlistEditor(); return; }
        tempoDrag = tempoArea().contains(e.position);
        tdrag = 0.0f;
    }
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
        menu->setBounds(getWidth() - 96, 22, 80, 40);
        maxi->setBounds(getWidth() - 146, 22, 42, 40);
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
        g.drawText("BY FLEXOR", 100, 48, 280, 14, juce::Justification::centredLeft);
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
        g.drawText("BREAK  (CLIC: LA LISTE)", 730, 8, 200, 12, juce::Justification::centredLeft);
        g.drawText("PATTERN", 950, 14, 80, 12, juce::Justification::centredLeft);
        if (brkHover == 1) { g.setColour(sk.panel2); g.fillRect(breakName()); }
        g.setColour(brkHover == 1 ? sk.hot : sk.ink);
        ui::fitText(g, juce::String(m.brk) + juce::String(juce::CharPointer_UTF8(" \xe2\x96\xbe")), breakName().reduced(4.0f, 0.0f), ui::value(32.0f), juce::Justification::centredLeft);
        {
            const auto pl = ed.proc.playlist();
            const int at = pl.indexOf(juce::String(m.brk).trim());
            ui::button(g, breakPrev(), "<", false, brkHover == 2, 12.0f);
            ui::button(g, breakNext(), ">", false, brkHover == 4, 12.0f);
            ui::button(g, breakList(), pl.isEmpty() ? juce::String("PLAYLIST (VIDE)") : "PLAYLIST " + (at >= 0 ? juce::String(at + 1) : juce::String("-")) + "/" + juce::String(pl.size()),
                       false, brkHover == 3, 10.5f);
        }
        g.setFont(ui::value(38.0f));
        g.drawText("P" + juce::String(m.pattern).paddedLeft('0', 2), 948, 26, 80, 40, juce::Justification::centredLeft);
        // the FM-1's message, or the VST's own
        juce::String a = m.say[0], b = m.say[1];
        if (juce::Time::getMillisecondCounterHiRes() < ed.toastUntil) { a = ed.toastText; b = {}; }
        const float dxr = (float) getWidth() - (float) W;   // (the right side follows the window's width)
        juce::Rectangle<float> msg(1036 + dxr, 14, 210, 56);
        g.setColour(sk.panel);
        g.fillRect(msg);
        g.setColour(sk.line);
        g.drawRect(msg, 1.0f);
        g.setColour(sk.accent);
        ui::fitText(g, a, msg.withHeight(28).reduced(6, 0), ui::label(11.0f));
        g.setColour(sk.ink);
        ui::fitText(g, b, msg.withTrimmedTop(28).reduced(6, 0), ui::value(22.0f));
        if (ed.proc.controller()) {      // (the FM-1 as the controller: playing it, or waiting for it)
            const bool on = ed.proc.controllerState() == 2;
            g.setColour(on ? sk.hot : sk.dim);
            g.setFont(ui::labelBold(9.0f));
            g.drawText(on ? juce::String(juce::CharPointer_UTF8("FM-1 \xe2\x97\x8f")) : juce::String("FM-1 ..."), msg.withHeight(14).reduced(5, 0),
                       juce::Justification::topRight);
        }
        // the meters
        for (int c = 0; c < 2; ++c) {
            const float v = c ? vr : vl, hld = c ? hr : hl;
            juce::Rectangle<float> bar(1258.0f + dxr + c * 16.0f, 12, 12, 60);
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
        ui::fitText(g, path, { (float) getWidth() - 300.0f, 6.0f, 290.0f, 32.0f }, ui::label(10.0f), juce::Justification::centredRight);
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
            if (r.getRight() > (float) getWidth() - 410.0f) break;
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
        return { X[i] + (float) getWidth() - (float) W, 4.0f, Wd[i], 26.0f };
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
// the break, big and detailed (FLEXOR, 25 Nov 2026: "pour les breaks j'aimerais une waveform plus détaillée"): each
// pixel column its min and max, coloured as the FM-1's RGB view (the lows red, the mids green, the highs blue), the
// slices numbered with their keys, the 32 steps, the playhead; a click plays the slice
struct WavePanel : Part
{
    explicit WavePanel(LBE& e) : Part(e) {}
    int held = -1;
    juce::Rectangle<float> area() const { return { 8.0f, 26.0f, (float) getWidth() - 16.0f, (float) getHeight() - 62.0f }; }
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        auto r = getLocalBounds().toFloat();
        g.setColour(sk.bg.brighter(0.02f));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(sk.line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
        const auto& w = ed.wave;
        const int ns = (int) w.slices.size();
        g.setColour(sk.dim);
        g.setFont(ui::label(8.5f).withExtraKerningFactor(0.14f));
        g.drawText("BREAK  /  " + juce::String(ed.m.brk) + "  /  " + juce::String(ns) + " SLICES", 14, 4, 600, 20, juce::Justification::centredLeft);
        auto a = area();
        const float mid = a.getCentreY(), half = a.getHeight() * 0.5f - 2.0f;
        const float play = ed.m.playing && ed.m.step >= 0 ? (float) (ed.m.step % 32) / 32.0f : -1.0f;
        // the slices: the one playing (or held) lit, every fourth a darker line
        for (int i = 0; i < ns; ++i) {
            const float x0 = a.getX() + w.slices[(size_t) i] * a.getWidth();
            const float x1 = i + 1 < ns ? a.getX() + w.slices[(size_t) i + 1] * a.getWidth() : a.getRight();
            const bool now = i == ed.m.slice || i == held;
            if (now) { g.setColour(sk.accent.withAlpha(0.08f)); g.fillRect(x0, a.getY(), x1 - x0, a.getHeight()); }
            g.setColour(i % 4 == 0 ? sk.line.brighter(0.2f) : sk.line.withAlpha(0.6f));
            g.fillRect(x0, a.getY(), 1.0f, a.getHeight());
        }
        g.setColour(sk.line.withAlpha(0.7f));
        g.fillRect(a.getX(), mid, a.getWidth(), 1.0f);
        const int n = (int) w.mm.size() / 2, nb = (int) w.cols.size() / 4;
        if (n > 0 && nb > 0) {
            for (int x = 0; x < (int) a.getWidth(); ++x) {
                const int c = juce::jlimit(0, n - 1, (int) ((float) x / a.getWidth() * n));
                const int cb = juce::jlimit(0, nb - 1, (int) ((float) x / a.getWidth() * nb));
                const float lo = w.cols[(size_t) (4 * cb + 1)], mi = w.cols[(size_t) (4 * cb + 2)], hi = w.cols[(size_t) (4 * cb + 3)];
                const float mn = w.mm[(size_t) (2 * c)], mx = w.mm[(size_t) (2 * c + 1)];
                const bool past = play >= 0.0f && (float) x / a.getWidth() > play;
                g.setColour(ui::rgb(lo, mi, hi, past ? 0.55f : 1.0f));
                const float y0 = mid - juce::jlimit(-1.0f, 1.0f, mx) * half, y1 = mid - juce::jlimit(-1.0f, 1.0f, mn) * half;
                g.fillRect(a.getX() + (float) x, y0, 1.0f, juce::jmax(1.0f, y1 - y0));
            }
        }
        for (int i = 0; i < ns; ++i) {           // (the slices' numbers, over the wave)
            const float x0 = a.getX() + w.slices[(size_t) i] * a.getWidth();
            const bool now = i == ed.m.slice || i == held;
            g.setColour(sk.bg.withAlpha(0.7f));
            g.fillRoundedRectangle((float) x0 + 3.0f, a.getY() + 2.0f, i >= 9 ? 18.0f : 12.0f, 14.0f, 2.0f);
            g.setColour(now ? sk.hot : sk.dim);
            g.setFont(ui::labelBold(9.0f));
            g.drawText(juce::String(i + 1), (int) x0 + 3, (int) a.getY() + 2, i >= 9 ? 18 : 12, 14, juce::Justification::centred);
        }
        if (play >= 0.0f) {
            g.setColour(sk.white.withAlpha(0.25f));
            g.fillRect(a.getX() + play * a.getWidth() - 3.0f, a.getY(), 8.0f, a.getHeight());
            g.setColour(sk.white);
            g.fillRect(a.getX() + play * a.getWidth(), a.getY(), 2.0f, a.getHeight());
        }
        // the 32 steps, then the slices' keys
        const float sw = a.getWidth() / 32.0f;
        for (int i = 0; i < 32; ++i) {
            juce::Rectangle<float> c(a.getX() + i * sw + 1.0f, a.getBottom() + 8.0f, sw - 3.0f, 9.0f);
            const bool now = ed.m.playing && ed.m.step >= 0 && ed.m.step % 32 == i;
            g.setColour(now ? sk.hot : i % 4 == 0 ? sk.line.brighter(0.3f) : sk.line);
            g.fillRoundedRectangle(c, 1.5f);
        }
        auto& km = KeyMap::get();
        const float kw = a.getWidth() / 16.0f;
        g.setFont(ui::labelBold(9.0f));
        for (int i = 0; i < 16; ++i) {
            g.setColour(i == ed.m.key ? sk.hot : sk.dim.withAlpha(0.7f));
            g.drawText(km.labelOf(KeyAction::White, i), juce::Rectangle<float>(a.getX() + i * kw, a.getBottom() + 20.0f, kw, 14.0f), juce::Justification::centred);
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

static std::optional<float> iconAt(const juce::String& v, float u);
// ------------------------------------------------------------------------------------ the sound, drawn big ---
// (FLEXOR, 25 Nov 2026: "dans le menu kick un vrai visuel sur le kick, une vraie waveform"): the kick (or the drum) as
// the FM-1's engine plays it (lb_pic: its kick_wave and kick_rgb), the view zoomed on the tab's part of it (ATTACK its
// first 60 ms, BODY 400 ms, TAIL the whole kick), its END marked; beside it the kick's card (its 20 memories, ECOUTER,
// COPIER, its layers) or the drums' (SN HH CP, their presets, their steps)
struct SoundHero : Part
{
    explicit SoundHero(LBE& e) : Part(e) {}
    int what = 0;                         // 0 the kick, 1..3 SN HH CP
    static constexpr float X0 = 16.0f, CY = 124.0f, A = 94.0f;
    float SW = 1032.0f, WW = 1000.0f;    // (the picture's width: the window's, the card's 384 px beside it)
    void resized() override { SW = (float) getWidth() - 392.0f; WW = SW - 32.0f; }
    static int autoMs(const juce::String& tab, int what)
    {
        if (what) return what == 2 ? 300 : 400;
        if (tab == "ATTACK") return 60;
        if (tab == "BODY" || tab == "DIRT") return 400;
        if (tab == "TAIL") return 1500;
        return 700;
    }
    int ms() const { return ed.picZoom > 0 ? ed.picZoom : autoMs(tabName(), what); }
    juce::String tabName() const { return ed.m.tab_on >= 0 && ed.m.tab_on < ed.m.ntabs ? juce::String(ed.m.tabs[ed.m.tab_on]) : juce::String(); }
    juce::Rectangle<float> zoomChip(int i) const { return { SW - 4.0f - (4 - i) * 54.0f, 5.0f, 50.0f, 17.0f }; }
    juce::Rectangle<float> mem(int i) const { const float w = 34.0f; return { SW + 8.0f + 16.0f + (i % 10) * (w + 1.5f), 70.0f + (i / 10) * 23.0f, w, 20.0f }; }
    juce::Rectangle<float> btn(int i) const { const float w = 112.0f; return { SW + 8.0f + 16.0f + i * (w + 5.0f), 124.0f, w, 32.0f }; }
    juce::Rectangle<float> drumRow(int i) const { return { SW + 8.0f + 12.0f, 12.0f + i * 80.0f, 360.0f, 74.0f }; }
    float endMs() const                   // TAIL's END, from the tab's parts (OFF: none)
    {
        for (auto& p : ed.parts.parts)
            for (int k = 0; k < 4; ++k)
                if (juce::String(p.kname[k]) == "END") {
                    const juce::String v(p.kval[k]);
                    if (v.containsOnly("0123456789MS ")) return v.getFloatValue();
                }
        return -1.0f;
    }
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        const auto& p = ed.pic;
        juce::Rectangle<float> sc(0, 0, SW, (float) getHeight());
        g.setColour(sk.bg.brighter(0.02f));
        g.fillRoundedRectangle(sc, 6.0f);
        g.setColour(sk.line);
        g.drawRoundedRectangle(sc.reduced(0.5f), 6.0f, 1.0f);
        const int m = ms();
        static const char* DN[3] = { "SNARE", "HAT", "CLAP" };
        juce::String t = what ? juce::String(DN[(what - 1) % 3]) + "  /  " + juce::String(ed.m.dpre[(what - 1) % 3])
                              : "KICK " + juce::String(ed.m.kslot) + "  /  " + juce::String(ed.m.kpre) + "  /  " + tabName();
        t << "  /  " << (m >= 1000 ? juce::String(m / 1000.0, 1) + " S" : juce::String(m) + " MS");
        g.setColour(sk.dim);
        g.setFont(ui::label(8.5f).withExtraKerningFactor(0.14f));
        g.drawText(t, 14, 4, 700, 20, juce::Justification::centredLeft);
        static const char* Z[4] = { "60 MS", "400 MS", "1.5 S", "AUTO" };
        static const int ZM[4] = { 60, 400, 1500, 0 };
        for (int i = 0; i < 4; ++i) {
            const bool on = ed.picZoom == ZM[i];
            auto c = zoomChip(i);
            g.setColour(on ? sk.ink : sk.line);
            if (on) g.fillRoundedRectangle(c, 2.0f); else g.drawRoundedRectangle(c, 2.0f, 1.0f);
            g.setColour(on ? sk.bg : sk.dim);
            g.setFont(ui::labelBold(8.0f));
            g.drawText(Z[i], c, juce::Justification::centred);
        }
        g.setColour(sk.line.withAlpha(0.8f));
        g.fillRect(X0, CY, WW, 1.0f);
        const bool fresh = p.what == what && p.ms == m && p.n > 0;
        if (fresh) {
            const int n = p.n;
            juce::Path outline;
            for (int x = 0; x < (int) WW; ++x) {
                const int c = juce::jlimit(0, n - 1, (int) ((float) x / WW * n));
                const float* v = &p.cols[(size_t) (5 * c)];
                const auto col = ui::rgb(v[1], v[2], v[3]);
                if (what == 0) {                 // the kick: its wave, cycle by cycle, filled from the middle
                    const float y = CY - juce::jlimit(-1.0f, 1.0f, v[0]) * A;
                    g.setColour(col.withAlpha(0.35f));
                    g.fillRect(X0 + x, juce::jmin(y, CY - v[4] * A), 1.0f, std::abs(y - (CY - v[4] * A)) + 0.0f);
                    g.setColour(col);
                    g.fillRect(X0 + x, juce::jmin(y, CY), 1.0f, juce::jmax(1.0f, std::abs(y - CY)));
                    if (x == 0) outline.startNewSubPath(X0, CY - v[4] * A); else outline.lineTo(X0 + x, CY - v[4] * A);
                } else {                         // a drum: its level, both ways
                    const float h = juce::jmax(1.0f, v[4] * A);
                    g.setColour(col);
                    g.fillRect(X0 + x, CY - h, 1.0f, 2.0f * h);
                }
            }
            if (what == 0) {
                g.setColour(sk.white.withAlpha(0.35f));
                g.strokePath(outline, juce::PathStrokeType(1.0f));
            }
        } else {
            g.setColour(sk.dim.withAlpha(0.6f));
            g.setFont(ui::label(10.0f));
            g.drawText("...", juce::Rectangle<float>(X0, CY - 20, WW, 40), juce::Justification::centred);
        }
        if (what == 0 && tabName() == "DIRT") {  // D+ TYPE, big: the curve the kick goes through (its DRIVE bending it)
            juce::String type;
            float drive = 0.0f;
            for (auto& pp : ed.parts.parts)
                for (int k = 0; k < 4; ++k) {
                    if (juce::String(pp.part) == "DISTO +" && juce::String(pp.kname[k]) == "TYPE") type = pp.kval[k];
                    if (juce::String(pp.part) == "DISTO +" && juce::String(pp.kname[k]) == "DRIVE") drive = juce::String(pp.kval[k]).getFloatValue();
                }
            if (type.isNotEmpty() && iconAt(type, 0.5f)) {
                juce::Rectangle<float> b(SW - 182.0f, 30.0f, 168.0f, 168.0f);
                g.setColour(sk.bg.withAlpha(0.88f));
                g.fillRoundedRectangle(b, 4.0f);
                g.setColour(sk.line);
                g.drawRoundedRectangle(b, 4.0f, 1.0f);
                g.setColour(sk.line.withAlpha(0.6f));
                g.drawLine(b.getCentreX(), b.getY() + 6, b.getCentreX(), b.getBottom() - 6, 1.0f);
                g.drawLine(b.getX() + 6, b.getCentreY(), b.getRight() - 6, b.getCentreY(), 1.0f);
                g.drawLine(b.getX() + 10, b.getBottom() - 10, b.getRight() - 10, b.getY() + 10, 1.0f);
                const float k = 1.0f + drive / 50.0f * 0.6f;   // (DRIVE -50 .. +50: the curve less or more pushed)
                juce::Path c;
                for (int i = 0; i <= 80; ++i) {
                    const float x = -1.0f + 2.0f * (float) i / 80.0f;
                    const float u = juce::jlimit(0.0f, 1.0f, (x * k + 1.0f) * 0.5f);
                    const float y = juce::jlimit(-1.0f, 1.0f, iconAt(type, u).value_or(0.0f));
                    const float px = b.getX() + 10.0f + (float) i / 80.0f * (b.getWidth() - 20.0f), py = b.getCentreY() - y * (b.getHeight() * 0.5f - 10.0f);
                    if (i == 0) c.startNewSubPath(px, py); else c.lineTo(px, py);
                }
                g.setColour(sk.hot);
                g.strokePath(c, juce::PathStrokeType(2.0f));
                g.setFont(ui::labelBold(9.0f));
                g.drawText("D+ TYPE  " + type, b.reduced(8.0f, 4.0f), juce::Justification::topLeft);
                g.setColour(sk.dim);
                g.setFont(ui::label(7.5f));
                g.drawText("ENTREE  >  SORTIE", b.reduced(8.0f, 4.0f), juce::Justification::bottomRight);
            }
        }
        if (what == 0 && tabName() == "TAIL") {  // the END, where it cuts
            const float e = endMs();
            if (e > 0.0f && e < (float) m) {
                const float x = X0 + e / (float) m * WW;
                g.setColour(sk.hot);
                for (float y = 30.0f; y < 220.0f; y += 9.0f) g.fillRect(x, y, 1.5f, 5.0f);
                g.setFont(ui::labelBold(9.0f));
                g.drawText("END  " + juce::String((int) e) + " MS", (int) x + 6, 28, 140, 14, juce::Justification::centredLeft);
            }
        }
        // the time
        g.setColour(sk.line.withAlpha(0.8f));
        g.fillRect(0.0f, 226.0f, SW, 1.0f);
        const int stepMs = m <= 60 ? 10 : m <= 300 ? 50 : m <= 400 ? 50 : m <= 700 ? 100 : 250;
        g.setFont(ui::label(8.0f));
        for (int tt = 0; tt <= m; tt += stepMs) {
            const float x = X0 + (float) tt / (float) m * WW;
            g.setColour(sk.line.brighter(0.15f));
            g.fillRect(x, 228.0f, 1.0f, 6.0f);
            g.setColour(sk.dim.withAlpha(0.7f));
            g.drawText(tt >= 1000 ? juce::String(tt / 1000.0, tt % 1000 ? 2 : 0) + " S" : juce::String(tt) + " MS", (int) x + 3, 234, 60, 14, juce::Justification::centredLeft);
        }
        // the card beside it
        juce::Rectangle<float> cd(SW + 8.0f, 0.0f, (float) getWidth() - SW - 8.0f, (float) getHeight());
        ui::card(g, cd, {});
        if (what == 0) paintKickCard(g, cd); else paintDrumCard(g, cd);
    }
    int hover = -1;
    void paintKickCard(juce::Graphics& g, juce::Rectangle<float> cd)
    {
        const auto& sk = s();
        g.setColour(sk.hot);
        g.setFont(ui::labelBold(30.0f));
        const juce::String k = "KICK " + juce::String(ed.m.kslot);
        g.drawText(k, (int) cd.getX() + 16, 8, 160, 52, juce::Justification::centredLeft);
        g.setColour(sk.ink.withAlpha(0.8f));
        g.setFont(ui::labelBold(14.0f));
        g.drawText(ed.m.kpre, (int) cd.getX() + 150, 18, 220, 40, juce::Justification::centredLeft);
        for (int i = 0; i < 20; ++i) {
            const auto r = mem(i);
            const bool on = i + 1 == ed.m.kslot;
            g.setColour(on ? sk.ink : hover == i ? sk.panel2.brighter(0.15f) : sk.panel2);
            g.fillRoundedRectangle(r, 2.0f);
            g.setColour(on ? sk.bg : sk.dim);
            g.setFont(ui::labelBold(8.5f));
            g.drawText(juce::String(i + 1), r, juce::Justification::centred);
        }
        static const char* B[3] = { "ECOUTER", "COPIER / COLLER", "SAVE" };
        for (int i = 0; i < 3; ++i)
            ui::button(g, btn(i), B[i], i == 0, hover == 100 + i, 10.5f);
        static const char* L[5] = { "SUB", "PUNCH", "GRIT", "CLICK", "TOK" };
        const float bw = (cd.getWidth() - 32.0f - 4 * 10.0f) / 5.0f;
        for (int i = 0; i < 5; ++i) {
            juce::Rectangle<float> b(cd.getX() + 16.0f + i * (bw + 10.0f), 168.0f, bw, 58.0f);
            g.setColour(sk.panel2);
            g.fillRoundedRectangle(b, 2.0f);
            const auto c = ui::layer(L[i]);
            g.setColour(c.withAlpha(0.85f));
            const float h = juce::jlimit(0.0f, 1.0f, ed.m.klev[i]) * b.getHeight();
            g.fillRect(b.withTop(b.getBottom() - h));
            g.setColour(c);
            g.setFont(ui::labelBold(8.5f));
            g.drawText(L[i], juce::Rectangle<float>(b.getX(), 230.0f, bw, 16.0f), juce::Justification::centred);
        }
    }
    void paintDrumCard(juce::Graphics& g, juce::Rectangle<float>)
    {
        const auto& sk = s();
        static const char* N[3] = { "SN", "HH", "CP" };
        for (int i = 0; i < 3; ++i) {
            auto r = drumRow(i);
            const bool on = i + 1 == what;
            g.setColour(on ? sk.panel2.brighter(0.06f) : sk.panel);
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(on ? sk.ink : sk.line);
            g.drawRoundedRectangle(r, 3.0f, 1.0f);
            g.setColour(on ? sk.hot : sk.dim);
            g.setFont(ui::labelBold(14.0f));
            g.drawText(N[i], (int) r.getX() + 10, (int) r.getY() + 6, 50, 30, juce::Justification::centredLeft);
            g.setColour(on ? sk.ink : sk.dim);
            g.setFont(ui::label(10.5f));
            g.drawText(juce::String(ed.m.dpre[i]) + "  /  " + juce::String((int) std::lround(ed.m.dlev[i] * 100.0f)) + "%",
                       (int) r.getX() + 60, (int) r.getY() + 6, 200, 30, juce::Justification::centredLeft);
            ui::button(g, hearBtn(i), "ECOUTER", false, hover == 200 + i, 9.0f);
            // its steps in the pattern (its first bar)
            const float sw = (r.getWidth() - 20.0f) / 16.0f;
            for (int st = 0; st < 16; ++st) {
                const bool hit = (ed.seq.mask[(i + 1) % 4][0] >> st) & 1u;
                g.setColour(hit ? (on ? sk.ink : sk.dim) : sk.panel2.brighter(0.04f));
                g.fillRoundedRectangle(r.getX() + 10.0f + st * sw, r.getBottom() - 22.0f, sw - 2.0f, 12.0f, 1.5f);
            }
        }
    }
    juce::Rectangle<float> hearBtn(int i) const { auto r = drumRow(i); return { r.getRight() - 84.0f, r.getY() + 8.0f, 74.0f, 24.0f }; }
    int hit(juce::Point<float> p) const
    {
        for (int i = 0; i < 4; ++i) if (zoomChip(i).contains(p)) return 300 + i;
        if (what == 0) {
            for (int i = 0; i < 20; ++i) if (mem(i).contains(p)) return i;
            for (int i = 0; i < 3; ++i) if (btn(i).contains(p)) return 100 + i;
        } else {
            for (int i = 0; i < 3; ++i) if (hearBtn(i).contains(p)) return 200 + i;
            for (int i = 0; i < 3; ++i) if (drumRow(i).contains(p)) return 210 + i;
        }
        if (p.x < SW) return 400;               // (the picture: heard)
        return -1;
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = hit(e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int h = hit(e.position);
        static const int ZM[4] = { 60, 400, 1500, 0 };
        if (h >= 300 && h < 304) { ed.picZoom = ZM[h - 300]; repaint(); return; }
        if (h == 400) { ed.proc.hear(what); return; }
        if (what == 0) {
            if (h >= 0 && h < 20 && h + 1 != ed.m.kslot) ed.turn(LB_PRESET, h + 1 - ed.m.kslot);   // (PRESETS: the kick, here)
            else if (h == 100) ed.proc.hear(0);
            else if (h == 101) { ed.hold(LB_SEL, true); ed.tap(LB_ARP); juce::Timer::callAfterDelay(120, [this] { ed.hold(LB_SEL, false); }); }
            else if (h == 102) ed.tap(LB_SAVE);
        } else {
            if (h >= 200 && h < 203) ed.proc.hear(h - 199);
            else if (h >= 210 && h < 213 && h - 209 != what && ed.m.tab_on >= 0) ed.turn(LB_SELECT, (h - 209) - what);
        }
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

// the icons of some settings' values (TAIL's MODE and CURVE, D+ TYPE, TOK's TYPE, the layers' effects, a drum's DIRT):
// each a little picture of what it does, over u 0..1 (nullopt: no icon)
static std::optional<float> iconAt(const juce::String& v, float u)
{
    const float pi = juce::MathConstants<float>::pi, x = u * 2.0f - 1.0f;   // (a transfer curve: x in, y out)
    auto env = [&](float k) { return std::exp(-u * k); };
    auto clampf = [](float a) { return juce::jlimit(-1.0f, 1.0f, a); };
    if (v == "CLASSIC") return env(2.2f) * std::tanh(1.8f * std::sin(u * 40.0f));
    if (v == "CLEAN") return env(2.2f) * std::sin(u * 40.0f);
    if (v == "SPLIT") return env(2.2f) * (u < 0.35f ? std::tanh(2.0f * std::sin(u * 40.0f)) : std::sin(u * 40.0f));
    if (v == "DIRTY") return env(2.2f) * clampf(std::sin(u * 40.0f) * (0.4f + u * 3.0f) * 3.0f);
    if (v == "EXP") return env(4.0f) * 2.0f - 1.0f;
    if (v == "LINEAR") return 1.0f - 2.0f * u;
    if (v == "SNAP") return (u < 0.55f ? 1.0f - u * 0.25f : juce::jmax(0.0f, 0.86f - (u - 0.55f) * 6.0f)) * 2.0f - 1.0f;
    if (v == "SOFT") return std::cos(u * pi);
    if (v == "SHARP") return u < 0.5f ? 1.0f : -1.0f;
    if (v == "FOLD") return std::sin(x * 2.6f);
    if (v == "CLIP") return clampf(x * 1.8f);
    if (v == "CRUSH") return std::round(x * 3.0f) / 3.0f;
    if (v == "RECT") return std::abs(x) * 2.0f - 1.0f;
    if (v == "TUBE") return x > 0 ? std::tanh(x * 2.4f) : std::tanh(x * 1.1f) * 0.85f;
    if (v == "HALF") return juce::jmax(0.0f, x) * 2.0f - 0.3f;
    if (v == "SINE") return std::sin(x * pi * 0.5f);
    if (v == "WRAP") { float y = x * 1.8f; while (y > 1.0f) y -= 2.0f; while (y < -1.0f) y += 2.0f; return y; }
    if (v == "FUZZ") return (x < 0 ? -1.0f : 1.0f) * (1.0f - std::exp(-std::abs(x) * 5.0f));
    if (v == "SQUARE") return std::sin(u * 18.0f) > 0 ? 0.8f : -0.8f;
    if (v == "NOISE") return std::sin(u * 517.0f) * std::sin(u * 211.0f + 1.0f) * 0.9f;
    if (v == "DRIVE") return std::tanh(3.0f * std::sin(u * 2.0f * pi * 2.0f));
    if (v == "CRUNCH" || v == "BITS") return std::round(std::sin(u * 2.0f * pi * 2.0f) * 2.0f) / 2.0f;
    if (v == "FILTER") return u < 0.55f ? 0.6f : 0.6f - (u - 0.55f) * 3.5f;
    if (v == "RING MOD") return std::sin(u * 2.0f * pi * 7.0f) * std::sin(u * pi);
    if (v == "HARD") return clampf(std::sin(u * 2.0f * pi * 2.0f) * 3.0f);
    if (v == "OFF" || v == "EMPTY") return 0.0f;
    if (v == "PUNCH" || v == "MB COMP" || v == "MAGIC MBC") return (u < 0.15f ? u / 0.15f : 1.0f - (u - 0.15f) * 0.5f) * 1.6f - 0.8f;
    if (v == "FLANGER") return std::sin(u * 2.0f * pi * 3.0f + std::sin(u * 2.0f * pi) * 2.0f) * 0.8f;
    if (v == "TREMOLO") return std::sin(u * 2.0f * pi * 9.0f) * (0.5f + 0.4f * std::sin(u * 2.0f * pi * 2.0f));
    if (v == "REVERB") return std::sin(u * 70.0f) * std::exp(-u * 3.0f) * 0.9f;
    if (v == "DELAY") return std::fmod(u, 0.33f) < 0.05f ? 0.9f * (1.0f - u) : 0.0f;
    if (v == "EQ" || v == "MAGIC EQ") return std::sin(u * 2.0f * pi) * 0.6f;
    if (v == "TAPE") return std::tanh(2.0f * std::sin(u * 2.0f * pi * 2.0f + 0.3f * std::sin(u * 9.0f))) * 0.85f;
    return std::nullopt;
}
static bool hasIcons(const juce::StringArray& v)
{
    if (v.size() < 2 || v.size() > 12) return false;
    for (auto& s : v) if (! iconAt(s.trim(), 0.5f)) return false;
    return true;
}
static void drawIcon(juce::Graphics& g, const juce::String& v, juce::Rectangle<float> r, juce::Colour c)
{
    juce::Path p;
    for (int i = 0; i <= 40; ++i) {
        const float u = (float) i / 40.0f;
        const float y = r.getCentreY() - juce::jlimit(-1.0f, 1.0f, iconAt(v.trim(), u).value_or(0.0f)) * r.getHeight() * 0.45f;
        if (i == 0) p.startNewSubPath(r.getX() + u * r.getWidth(), y); else p.lineTo(r.getX() + u * r.getWidth(), y);
    }
    g.setColour(c);
    g.strokePath(p, juce::PathStrokeType(1.4f));
}

struct MenuContent : Part
{
    explicit MenuContent(LBE& e) : Part(e) {}
    struct Param {
        int part = 0, knob = 0;
        juce::String key, name, val;
        juce::Rectangle<float> row, ctl;
        menus::Kind kind = menus::Unknown;
        int cur = -1, n = 0;
        int off = -1;                    // (a long slider's OFF: a wider stretch of it, shaded)
        float offW = 1.0f;
        int centre = -1;                 // (a value both ways, -50 .. +50: filled from its 0)
        bool wrap = false;
        bool tiles = false;              // (its values as pictures: TAIL's MODE, D+ TYPE ...)
        bool listed = false;             // (a part of one list: its values listed whole, PRESET)
        bool fxPick = false;             // (an effect's page: the effect, picked by PRESETS)
    };
    struct Card { juce::String title, sub; juce::Colour chip; juce::Rectangle<float> r; bool live = false, faders = false, rings = false, eq = false; std::vector<Param> ps; };
    // an EQ's bands (the kick's EQ BANDS, an EQ in the chain): its curve, its three points dragged
    static bool eqFor(const Card& c)
    {
        int n = 0;
        for (auto& q : c.ps)
            if ((q.name == "LOW" || q.name == "MID" || q.name == "HIGH") && q.val.containsIgnoreCase("DB")) n++;
        return n == 3;
    }
    static float parseHz(const juce::String& v) { const auto t = v.toUpperCase(); return t.getFloatValue() * (t.contains("KHZ") ? 1000.0f : 1.0f); }
    const Param* findP(const juce::String& name) const { for (auto& c : cards) for (auto& q : c.ps) if (q.name == name) return &q; return nullptr; }
    Param* findPm(const juce::String& name) { for (auto& c : cards) for (auto& q : c.ps) if (q.name == name) return &q; return nullptr; }
    struct EqPt { float hz, db; juce::String gain, freq; };
    std::array<EqPt, 3> eqPoints() const
    {
        auto v = [&](const juce::String& n, float d) { auto* q = findP(n); return q ? q->val.getFloatValue() : d; };
        auto hz = [&](const juce::String& n, float d) { auto* q = findP(n); return q ? parseHz(q->val) : d; };
        return { EqPt { hz("LO FREQ", 126.0f), v("LOW", 0), "LOW", "LO FREQ" }, EqPt { hz("MID FREQ", 999.0f), v("MID", 0), "MID", "MID FREQ" },
                 EqPt { hz("HI FREQ", 5700.0f), v("HIGH", 0), "HIGH", "HI FREQ" } };
    }
    float eqGain(float hz) const
    {
        const auto p = eqPoints();
        auto* qq = findP("MID Q");
        const float q = qq ? juce::jmax(0.2f, qq->val.getFloatValue()) : 1.1f;
        const float lo = p[0].db / (1.0f + std::pow(hz / p[0].hz, 2.0f)), hi = p[2].db / (1.0f + std::pow(p[2].hz / hz, 2.0f));
        const float mid = p[1].db / (1.0f + std::pow(q * (hz / p[1].hz - p[1].hz / hz), 2.0f));
        auto* lv = findP("LEVEL");
        return lo + mid + hi + (lv && lv->val.containsIgnoreCase("DB") ? lv->val.getFloatValue() : 0.0f);
    }
    juce::Rectangle<float> eqArea(const Card& c) const { return { c.r.getX() + 14.0f, c.r.getY() + HEAD_H, c.r.getWidth() - 28.0f, c.r.getHeight() - HEAD_H - 44.0f }; }
    static float eqX(juce::Rectangle<float> a, float hz) { return a.getX() + std::log(hz / 20.0f) / std::log(1000.0f) * a.getWidth(); }
    static float eqY(juce::Rectangle<float> a, float db) { return a.getCentreY() - db / 15.0f * (a.getHeight() * 0.5f - 6.0f); }
    int eqDrag = -1, eqCardI = -1;          // (by index: the cards may be laid out again while it is dragged)
    // the FM-1's four knobs (MORE › KNOBS): round, as on the machine
    static bool ringsFor(const Card& c)
    {
        if (c.title != "KNOBS" || c.ps.size() != 4) return false;
        for (auto& q : c.ps) if (q.kind != menus::Slider) return false;
        return true;
    }
    // levels side by side (MIX › LEVELS, the kick's MIX, MACRO): faders, as on a mixer
    static bool fadersFor(const Card& c)
    {
        if (c.ps.size() < 2 || c.ps.size() > 5) return false;
        if (c.title != "MIX" && c.title != "LEVELS" && c.title != "MACRO") return false;
        for (auto& q : c.ps) if (q.kind != menus::Slider) return false;
        return true;
    }
    std::vector<Card> cards;
    int hoverCard = -1, hoverParam = -1, hoverOpt = -1;
    int focus = -1;                      // (the setting the keyboard has: its place among them all, -1 none)
    Param* nth(int i) { for (auto& c : cards) for (auto& q : c.ps) if (i-- == 0) return &q; return nullptr; }
    int count() const { int n = 0; for (auto& c : cards) n += (int) c.ps.size(); return n; }
    void command(int c)
    {
        using A = KeyAction;
        const int n = count();
        if (! n) return;
        if (focus < 0 || focus >= n) focus = 0;
        else if (c == A::MenuUp) focus = (focus + n - 1) % n;
        else if (c == A::MenuDown) focus = (focus + 1) % n;
        if (auto* q = nth(focus)) {
            if (c == A::MenuLeft || c == A::MenuRight) nudge(*q, c == A::MenuRight ? 1 : -1);
            else if (c == A::MenuBigLeft || c == A::MenuBigRight) {
                const int step = juce::jmax(1, q->n / 10);
                if (q->kind == menus::Unknown) nudge(*q, c == A::MenuBigRight ? 1 : -1);
                else go(*q, q->cur + (c == A::MenuBigRight ? step : -step));
            } else if (c == A::MenuOpen) {
                if (q->kind == menus::Toggle) go(*q, 1 - q->cur);
                else if (q->n > 1) listMenu(*q);
            }
            if (auto* vp = findParentComponentOfClass<juce::Viewport>())   // (kept in sight)
                if (! vp->getViewArea().contains(q->row.toNearestInt()))
                    vp->setViewPosition(0, juce::jmax(0, (int) q->row.getY() - 60));
        }
        repaint();
    }
    // a drag on a slider: the value it started at, the pixels a step
    Param* dragP = nullptr;
    int dragFrom = 0, dragSent = 0;
    float dragU = 0.0f;
    float pxStep = 8.0f;
    bool dragV = false;
    juce::String dragKey;                // (FLEXOR: "des faders restent bloqués après avoir un peu bougé": the cards are
                                         // laid out again while one is dragged; it is found again by its name)
    const Card* cardOf(const Param* q) const { for (auto& c : cards) for (auto& p : c.ps) if (&p == q) return &c; return nullptr; }

    int availW = 1400, availH = 390;
    static constexpr float ROW_H = 34.0f, HEAD_H = 34.0f, GAP = 8.0f, TILE_H = 46.0f;

    // a setting's height: a row; its values as pictures, rows of tiles; a list, all of it
    float paramH(const Param& q) const
    {
        if (q.listed) return juce::jmax(ROW_H, (float) ((q.n + listCols(q) - 1) / listCols(q)) * 19.0f + 6.0f);
        if (q.tiles) return 18.0f + (float) ((q.n + 4) / 5) * (TILE_H + 4.0f);
        return ROW_H;
    }
    // the cards again from the scan (lb_scan's order: the part shown on the FM-1 first; kept in the page's first order),
    // laid out to fill the room: a few cards side by side, more in rows, a list (PRESET) a column of its own
    void rebuild()
    {
        cards.clear();
        const auto& ps = ed.parts.parts;
        const int n = (int) ps.size();
        if (n == 0) { setSize(availW, 10); repaint(); return; }
        int first = 0;
        const auto ord = order.find(ed.pageKey());
        if (ord != order.end()) {
            for (int i = 0; i < n; ++i) if (ident(ps[(size_t) i]) == ord->second) { first = i; break; }
        } else if (n > 1)
            order[ed.pageKey()] = ident(ps[0]);
        // the cards and their settings, unplaced
        for (int j = 0; j < n; ++j) {
            const int i = (first + j) % n;
            const auto& p = ps[(size_t) i];
            Card c;
            c.title = p.part[0] ? juce::String(p.part) : n == 1 ? tabName() : juce::String("PLUS");
            c.sub = ui::subtitle(tabName(), c.title);
            c.chip = ui::layer(c.title);
            c.live = i == 0 && n > 1;
            for (int k = 0; k < 4; ++k) {
                if (! p.kname[k][0]) continue;
                if (juce::String(p.kname[k]) == "PRESET" && c.title.endsWith(" FX")) continue;   // (the FM-1's spare K3: the kick)
                Param q;
                q.part = i;
                q.knob = k;
                q.name = p.kname[k];
                q.val = p.kval[k];
                q.key = ed.optKey(p, k);
                refreshParam(q);
                c.ps.push_back(q);
            }
            c.faders = fadersFor(c);
            c.rings = ringsFor(c);
            c.faders |= c.rings;                 // (laid out and dragged as faders: columns, up / down)
            c.eq = eqFor(c);
            int shown = 0;
            for (auto& q : c.ps) shown += q.name.isNotEmpty();
            if (shown == 1 && c.ps.size() == 1 && c.ps[0].kind == menus::List && c.ps[0].n > 1 && c.ps[0].n <= 40)
                c.ps[0].listed = true;   // (one list alone, PRESET: its values listed whole, a column)
            cards.push_back(std::move(c));
        }
        // an effect's page (a kick's FX1 / FX2, a slot of the chain): the effect itself first, every one listed (PRESETS)
        if (! ed.parts.fx.isEmpty()) {
            Card c;
            c.title = "EFFET";
            c.sub = "PRESETS";
            Param q;
            q.part = -1;
            q.fxPick = q.listed = true;
            q.kind = menus::List;
            q.n = ed.parts.fx.size();
            q.cur = ed.parts.fxCur;
            q.name = "EFFET";
            q.val = ed.parts.fx[juce::jmax(0, q.cur)];
            q.key = "fx|" + ed.pageKey();
            c.ps.push_back(q);
            cards.insert(cards.begin(), std::move(c));
        }
        // the columns: a listed card a narrow column of its own (left); the others in a grid
        const float avail = (float) availW - 4.0f;
        std::vector<int> listed, grid;
        for (int i = 0; i < (int) cards.size(); ++i) (cards[(size_t) i].ps.size() == 1 && cards[(size_t) i].ps[0].listed ? listed : grid).push_back(i);
        float x0 = 2.0f;
        float bottom = 0.0f;
        for (int li : listed) {
            auto& c = cards[(size_t) li];
            const float h = juce::jmax((float) availH - 6.0f, HEAD_H + paramH(c.ps[0]) + 10.0f);
            c.r = { x0, 2.0f, 290.0f, h };
            layoutCard(c);
            x0 += 290.0f + GAP;
            bottom = juce::jmax(bottom, c.r.getBottom());
        }
        const int ng = (int) grid.size();
        if (ng > 0) {
            const int cols = ng <= 3 ? ng : ng == 4 ? (avail - x0 > 1100.0f ? 4 : 2) : ng <= 6 ? 3 : 4;
            float cw = (avail - x0 - GAP * (cols - 1)) / cols;
            if (ng == 1) {                       // (one card alone: as wide as it needs, not the whole window)
                const auto& c0 = cards[(size_t) grid[0]];
                cw = juce::jmin(cw, c0.faders ? 130.0f * (float) c0.ps.size() + 40.0f : 760.0f);
            }
            const int rows = (ng + cols - 1) / cols;
            // each row as tall as its tallest card's settings; stretched to fill the room when they fit
            std::vector<float> need((size_t) rows, 0.0f);
            for (int j = 0; j < ng; ++j) {
                float h = HEAD_H + 10.0f;
                if (cards[(size_t) grid[(size_t) j]].faders) h += 140.0f;
                else if (cards[(size_t) grid[(size_t) j]].eq) h += 210.0f;
                else for (auto& q : cards[(size_t) grid[(size_t) j]].ps) h += paramH(q);
                need[(size_t) (j / cols)] = juce::jmax(need[(size_t) (j / cols)], h);
            }
            float total = GAP * (rows - 1);
            for (float h : need) total += h;
            const float stretch = total < (float) availH - 6.0f ? ((float) availH - 6.0f - GAP * (rows - 1)) / (total - GAP * (rows - 1)) : 1.0f;
            float y = 2.0f;
            for (int r = 0; r < rows; ++r) {
                const float rh = need[(size_t) r] * stretch;
                for (int col = 0; col < cols; ++col) {
                    const int j = r * cols + col;
                    if (j >= ng) break;
                    auto& c = cards[(size_t) grid[(size_t) j]];
                    c.r = { x0 + col * (cw + GAP), y, cw, rh };
                    layoutCard(c);
                }
                y += rh + GAP;
            }
            bottom = juce::jmax(bottom, y - GAP);
        }
        setSize(availW, (int) bottom + 6);
        dragP = nullptr;
        if (dragKey.isNotEmpty())
            for (auto& c : cards) for (auto& q : c.ps) if (q.key == dragKey) dragP = &q;
        repaint();
    }
    void layoutCard(Card& c)
    {
        if (c.eq) {                              // the curve above; each setting a value under it, dragged sideways
            const float n = (float) c.ps.size(), w = (c.r.getWidth() - 28.0f) / n, y = c.r.getBottom() - 40.0f;
            for (size_t i = 0; i < c.ps.size(); ++i) {
                auto& q = c.ps[i];
                q.row = { c.r.getX() + 14.0f + (float) i * w, y, w - 6.0f, 32.0f };
                q.ctl = q.row;
            }
            return;
        }
        if (c.faders) {                          // each a column: its value above, its track, its name below
            const float n = (float) c.ps.size(), w = (c.r.getWidth() - 28.0f) / n;
            for (size_t i = 0; i < c.ps.size(); ++i) {
                auto& q = c.ps[i];
                q.row = { c.r.getX() + 14.0f + (float) i * w, c.r.getY() + HEAD_H, w, c.r.getHeight() - HEAD_H - 10.0f };
                const float tw = juce::jmin(46.0f, w - 16.0f);
                q.ctl = juce::Rectangle<float>(q.row.getCentreX() - tw * 0.5f, q.row.getY() + 24.0f, tw, q.row.getHeight() - 48.0f);
                if (c.rings) {
                    const float d = juce::jmin(w - 20.0f, q.row.getHeight() - 56.0f, 110.0f);
                    q.ctl = juce::Rectangle<float>(d, d).withCentre({ q.row.getCentreX(), q.row.getY() + 8.0f + d * 0.5f });
                }
            }
            return;
        }
        float need = 0.0f;
        for (auto& q : c.ps) need += paramH(q);
        const float room = c.r.getHeight() - HEAD_H - 8.0f;
        const float grow = need > 0.0f && room > need ? juce::jmin(1.35f, room / need) : 1.0f;   // (rows a bit taller when there is room)
        float ry = c.r.getY() + HEAD_H;
        for (auto& q : c.ps) {
            const float h = paramH(q) * (q.tiles || q.listed ? 1.0f : grow);
            q.row = { c.r.getX() + 14.0f, ry, c.r.getWidth() - 28.0f, h - 6.0f };
            if (q.tiles || q.listed) q.ctl = q.row.withTrimmedTop(q.tiles ? 18.0f : 0.0f);
            else q.ctl = q.row.withTrimmedLeft(juce::jmin(110.0f, q.row.getWidth() * 0.3f));
            ry += h;
        }
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
        q.tiles = hasIcons(o.v) && q.n > 2;
        q.off = -1;
        q.offW = 1.0f;
        q.centre = -1;
        if (q.kind == menus::Slider && q.n > 12)   // (FLEXOR: "les curseurs qui ont pas mal de travel, comme les filtres,
                                                   // doivent avoir une zone OFF plus grande et délimitée discrètement")
            for (int i = 0; i < q.n; ++i)
                if (o.v[i].trim() == "OFF") { q.off = i; q.offW = juce::jmax(2.5f, 0.14f * (float) (q.n - 1)); break; }
        if (q.kind == menus::Slider && q.off < 0 && o.v[0].trimStart().startsWithChar('-') && o.v[q.n - 1].trimStart().startsWithChar('+'))
            for (int i = 0; i < q.n; ++i)        // (both ways: its 0, the middle)
                if (o.v[i].trim().getFloatValue() == 0.0f && o.v[i].containsAnyOf("0")) { q.centre = i; break; }
    }
    // the values only (the same parts): no relayout
    bool sameShape() const
    {
        const auto& ps = ed.parts.parts;
        size_t total = 0, ncards = 0;
        for (auto& c : cards) { if (! c.ps.empty() && c.ps[0].fxPick) continue; ncards++; for (auto& q : c.ps) total++; }
        size_t want = 0;
        for (auto& p : ps) for (int k = 0; k < 4; ++k)
            if (p.kname[k][0] && ! (juce::String(p.kname[k]) == "PRESET" && juce::String(p.part).endsWith(" FX"))) want++;
        if (total != want || ncards != ps.size()) return false;
        const bool fxCard = ! cards.empty() && ! cards[0].ps.empty() && cards[0].ps[0].fxPick;
        if (fxCard != ! ed.parts.fx.isEmpty()) return false;
        if (fxCard && cards[0].ps[0].n != ed.parts.fx.size()) return false;
        for (auto& c : cards) for (auto& q : c.ps) {
            if (q.fxPick) continue;
            if (q.part >= (int) ps.size()) return false;
            if (q.name != ps[(size_t) q.part].kname[q.knob] || q.key != ed.optKey(ps[(size_t) q.part], q.knob)) return false;
        }
        return true;
    }
    void update()
    {
        if (! sameShape()) { rebuild(); return; }
        bool relayout = false;
        for (auto& c : cards) for (auto& q : c.ps) {
            if (q.fxPick) { q.cur = ed.parts.fxCur; q.val = ed.parts.fx[juce::jmax(0, q.cur)]; continue; }
            if (&q == dragP) continue;
            const auto was = std::make_tuple(q.kind, q.tiles, q.listed, q.n);
            q.val = ed.parts.parts[(size_t) q.part].kval[q.knob];
            const bool listed = q.listed;
            refreshParam(q);
            q.listed = listed && q.kind == menus::List;
            if (! listed && c.ps.size() == 1 && q.kind == menus::List && q.n > 1 && q.n <= 40) q.listed = true;
            relayout |= was != std::make_tuple(q.kind, q.tiles, q.listed, q.n);
        }
        for (auto& c : cards) relayout |= c.rings != ringsFor(c) || c.faders != (fadersFor(c) || ringsFor(c)) || c.eq != eqFor(c);
        if (relayout) { rebuild(); return; }
        repaint();
    }
    // ---- drawing
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        for (int ci = 0; ci < (int) cards.size(); ++ci) {
            auto& c = cards[(size_t) ci];
            ui::card(g, c.r, c.title, c.sub, c.chip, c.live);
            if (c.eq) drawEq(g, c);
            for (int pi = 0; pi < (int) c.ps.size(); ++pi) drawParam(g, c, c.ps[(size_t) pi], hoverCard == ci && hoverParam == pi);
        }
        if (auto* f = focus >= 0 ? nth(focus) : nullptr) {   // (the keyboard's setting: a frame)
            g.setColour(sk.hot);
            g.drawRoundedRectangle(f->row.expanded(4.0f, 2.0f), 3.0f, 1.5f);
        }
        if (cards.empty()) {
            g.setColour(sk.dim);
            g.setFont(ui::label(14.0f));
            g.drawText("RIEN A REGLER ICI", getLocalBounds(), juce::Justification::centred);
        }
    }
    void drawEq(juce::Graphics& g, const Card& c)
    {
        const auto& sk = s();
        const auto a = eqArea(c);
        g.setColour(sk.bg.brighter(0.02f));
        g.fillRoundedRectangle(a, 3.0f);
        g.setFont(ui::label(7.5f));
        for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f }) {
            const float x = eqX(a, hz);
            g.setColour(sk.line.withAlpha(0.5f));
            g.fillRect(x, a.getY(), 1.0f, a.getHeight());
            g.setColour(sk.dim.withAlpha(0.6f));
            g.drawText(hz >= 1000.0f ? juce::String((int) (hz / 1000.0f)) + "K" : juce::String((int) hz), (int) x + 2, (int) a.getBottom() - 12, 30, 11, juce::Justification::centredLeft);
        }
        for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f }) {
            g.setColour(sk.line.withAlpha(db == 0.0f ? 0.9f : 0.4f));
            g.fillRect(a.getX(), eqY(a, db), a.getWidth(), 1.0f);
        }
        juce::Path p, f;
        for (int i = 0; i <= 200; ++i) {
            const float hz = 20.0f * std::pow(1000.0f, (float) i / 200.0f), x = eqX(a, hz), y = juce::jlimit(a.getY(), a.getBottom(), eqY(a, eqGain(hz)));
            if (i == 0) { p.startNewSubPath(x, y); f.startNewSubPath(x, eqY(a, 0.0f)); }
            p.lineTo(x, y);
            f.lineTo(x, y);
        }
        f.lineTo(a.getRight(), eqY(a, 0.0f));
        f.closeSubPath();
        g.setColour(sk.accent.withAlpha(0.12f));
        g.fillPath(f);
        g.setColour(sk.hot);
        g.strokePath(p, juce::PathStrokeType(2.0f));
        const auto pts = eqPoints();
        static const char* N[3] = { "LOW", "MID", "HIGH" };
        for (int i = 0; i < 3; ++i) {
            const float x = eqX(a, pts[(size_t) i].hz), y = juce::jlimit(a.getY() + 7.0f, a.getBottom() - 7.0f, eqY(a, pts[(size_t) i].db));
            g.setColour(sk.bg);
            g.fillEllipse(x - 7.0f, y - 7.0f, 14.0f, 14.0f);
            g.setColour(eqDrag == i && eqCardI >= 0 && eqCardI < (int) cards.size() && &cards[(size_t) eqCardI] == &c ? sk.white : sk.hot);
            g.drawEllipse(x - 7.0f, y - 7.0f, 14.0f, 14.0f, 2.0f);
            g.setColour(sk.ink);
            g.setFont(ui::labelBold(8.0f));
            g.drawText(N[i], (int) x - 20, (int) y - 22, 40, 12, juce::Justification::centred);
        }
    }
    juce::String optAt(const Param& q, int i) const
    {
        if (q.fxPick) return i >= 0 && i < ed.parts.fx.size() ? ed.parts.fx[i] : juce::String();
        auto it = ed.opts.find(q.key);
        return it != ed.opts.end() && i >= 0 && i < it->second.v.size() ? it->second.v[i] : juce::String();
    }
    juce::Rectangle<float> tileR(const Param& q, int i) const
    {
        const int cols = juce::jmin(5, q.n), row = i / cols, col = i % cols;
        const float w = (q.ctl.getWidth() - 4.0f * (cols - 1)) / cols;
        return { q.ctl.getX() + col * (w + 4.0f), q.ctl.getY() + row * (TILE_H + 4.0f), w, TILE_H };
    }
    static int listCols(const Param& q) { return q.n > 16 ? 2 : 1; }
    juce::Rectangle<float> listR(const Param& q, int i) const
    {
        const int cols = listCols(q), per = (q.n + cols - 1) / cols;
        const float rh = juce::jlimit(17.0f, 26.0f, q.ctl.getHeight() / (float) juce::jmax(1, per));
        const float w = (q.ctl.getWidth() - 4.0f * (cols - 1)) / cols;
        return { q.ctl.getX() + (i / per) * (w + 4.0f), q.ctl.getY() + (i % per) * rh, w, rh - 2.0f };
    }
    void drawParam(juce::Graphics& g, const Card& c, const Param& q, bool hover)
    {
        const auto& sk = s();
        const juce::Colour tint = c.chip.isTransparent() ? sk.accent : c.chip;
        if (c.rings) {                           // a knob: its arc, its value in it, its name under
            const float t = q.n > 1 ? frac(q, unitsAt(q, q.cur)) : 0.0f;
            const auto r = q.ctl.reduced(6.0f);
            const float a0 = juce::MathConstants<float>::pi * 1.25f, span = juce::MathConstants<float>::pi * 1.5f;
            juce::Path bg, val;
            bg.addCentredArc(r.getCentreX(), r.getCentreY(), r.getWidth() * 0.5f, r.getHeight() * 0.5f, 0.0f, -a0 + juce::MathConstants<float>::twoPi, -a0 + juce::MathConstants<float>::twoPi + span, true);
            g.setColour(sk.panel2.brighter(0.08f));
            g.strokePath(bg, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (t > 0.001f && ! (q.off >= 0 && q.cur == q.off)) {
                val.addCentredArc(r.getCentreX(), r.getCentreY(), r.getWidth() * 0.5f, r.getHeight() * 0.5f, 0.0f, -a0 + juce::MathConstants<float>::twoPi, -a0 + juce::MathConstants<float>::twoPi + span * t, true);
                g.setColour(hover || dragP == &q ? sk.hot : sk.ink);
                g.strokePath(val, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour(q.off >= 0 && q.cur == q.off ? sk.dim : sk.ink);
            ui::fitText(g, q.val, r.reduced(10.0f), ui::value(18.0f));
            g.setColour(sk.dim);
            g.setFont(ui::labelBold(8.5f));
            g.drawText("K" + juce::String(q.knob + 1), juce::Rectangle<float>(q.row.getX(), q.ctl.getBottom() + 2.0f, q.row.getWidth(), 14.0f), juce::Justification::centred);
            g.setColour(sk.ink);
            ui::fitText(g, q.name, juce::Rectangle<float>(q.row.getX(), q.ctl.getBottom() + 16.0f, q.row.getWidth(), 20.0f), ui::labelBold(11.0f));
            return;
        }
        if (c.eq) {                              // under the curve: a value, its name above it, dragged sideways
            const float t = q.n > 1 ? frac(q, unitsAt(q, q.cur)) : 0.0f;
            auto r = q.ctl;
            g.setColour(sk.panel2);
            g.fillRoundedRectangle(r, 2.5f);
            g.setColour(sk.accent.withAlpha(hover || dragP == &q ? 0.4f : 0.22f));
            if (q.centre >= 0) {
                const float from = frac(q, unitsAt(q, q.centre));
                g.fillRect(juce::Rectangle<float>(r.getX() + r.getWidth() * juce::jmin(from, t), r.getBottom() - 4.0f, r.getWidth() * std::abs(t - from), 4.0f));
            } else
                g.fillRect(r.withTop(r.getBottom() - 4.0f).withWidth(r.getWidth() * t));
            g.setColour(sk.dim);
            ui::fitText(g, q.name, r.withHeight(13.0f).reduced(5.0f, 0.0f), ui::label(7.5f), juce::Justification::centredLeft);
            g.setColour(sk.ink);
            ui::fitText(g, q.val, r.withTrimmedTop(11.0f).withTrimmedBottom(4.0f).reduced(5.0f, 0.0f), ui::value(14.0f), juce::Justification::centredLeft);
            return;
        }
        if (c.faders) {                          // a fader: filled from below, the layer's colour (SUB red ...)
            const auto lc = ui::layer(q.name);
            const auto col = lc.isTransparent() ? sk.ink : lc;
            const float t = q.n > 1 ? frac(q, unitsAt(q, q.cur)) : 0.0f;
            auto tr = q.ctl;
            g.setColour(sk.panel2);
            g.fillRoundedRectangle(tr, 3.0f);
            g.setColour(col.withAlpha(hover || dragP == &q ? 0.75f : 0.6f));
            g.fillRoundedRectangle(tr.withTop(tr.getBottom() - tr.getHeight() * t), 3.0f);
            for (int k = 1; k < 10; ++k) {       // (its scale)
                g.setColour(sk.bg.withAlpha(0.35f));
                g.fillRect(tr.getX(), tr.getBottom() - tr.getHeight() * k / 10.0f, tr.getWidth() * (k % 5 ? 0.25f : 0.5f), 1.0f);
            }
            g.setColour(sk.hot);
            g.fillRect(tr.getX() - 4.0f, tr.getBottom() - tr.getHeight() * t - 1.5f, tr.getWidth() + 8.0f, 3.0f);
            g.setColour(sk.ink);
            ui::fitText(g, q.val, q.row.withHeight(22.0f), ui::value(17.0f));
            g.setColour(lc.isTransparent() ? sk.dim : lc);
            ui::fitText(g, q.name, q.row.withTop(q.row.getBottom() - 20.0f), ui::labelBold(9.5f));
            return;
        }
        if (q.listed) {                          // a list, whole: each value a row, the one in use lit
            for (int i = 0; i < q.n; ++i) {
                auto r = listR(q, i);
                const bool on = i == q.cur, hv = hover && hoverOpt == i;
                g.setColour(on ? sk.ink : hv ? sk.panel2.brighter(0.06f) : sk.panel);
                g.fillRoundedRectangle(r, 3.0f);
                g.setColour(on ? sk.bg.withAlpha(0.6f) : sk.dim.withAlpha(0.7f));
                g.setFont(ui::label(8.5f));
                g.drawText(juce::String(i + 1), r.withWidth(26.0f), juce::Justification::centred);
                g.setColour(on ? sk.bg : sk.ink.withAlpha(0.9f));
                const auto v = optAt(q, i);
                const bool icon = q.fxPick && iconAt(v, 0.5f).has_value();
                ui::fitText(g, v, r.withTrimmedLeft(28.0f).withTrimmedRight(icon ? 50.0f : 8.0f), ui::labelBold(juce::jmin(10.5f, r.getHeight() * 0.45f)), juce::Justification::centredLeft);
                if (icon) drawIcon(g, v, r.withLeft(r.getRight() - 46.0f).reduced(4.0f, 3.0f), on ? sk.bg : sk.dim.withAlpha(0.7f));
            }
            return;
        }
        g.setColour(hover ? sk.ink : sk.dim);
        if (q.tiles) {
            ui::fitText(g, q.name, q.row.withHeight(16.0f), ui::label(9.0f), juce::Justification::centredLeft);
            for (int i = 0; i < q.n; ++i) {
                auto r = tileR(q, i);
                const bool on = i == q.cur, hv = hover && hoverOpt == i;
                g.setColour(on ? sk.panel2.brighter(0.08f) : hv ? sk.panel2.brighter(0.04f) : sk.panel2);
                g.fillRoundedRectangle(r, 3.0f);
                g.setColour(on ? sk.ink : sk.line);
                g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, on ? 1.5f : 1.0f);
                drawIcon(g, optAt(q, i), r.withTrimmedBottom(16.0f).reduced(10.0f, 5.0f), on ? tint.brighter(0.2f) : sk.dim.withAlpha(0.7f));
                g.setColour(on ? sk.hot : sk.dim);
                ui::fitText(g, optAt(q, i), r.withTrimmedTop(r.getHeight() - 17.0f).reduced(3.0f, 0.0f), ui::labelBold(8.5f));
            }
            return;
        }
        ui::fitText(g, q.name, q.row.withWidth(q.ctl.getX() - q.row.getX() - 6.0f), ui::label(10.0f), juce::Justification::centredLeft);
        auto r = q.ctl;
        const float th = juce::jmin(26.0f, r.getHeight() - 6.0f);
        switch (q.kind) {
        case menus::Toggle:
        case menus::Segments: {
            auto b = r.withSizeKeepingCentre(r.getWidth(), th);
            const float w = b.getWidth() / q.n;
            for (int i = 0; i < q.n; ++i) {
                juce::Rectangle<float> cell(b.getX() + i * w + (i ? 1.5f : 0.0f), b.getY(), w - 3.0f, b.getHeight());
                const bool on = i == q.cur;
                g.setColour(on ? sk.ink : sk.panel2);
                g.fillRoundedRectangle(cell, 2.5f);
                if (! on) { g.setColour(sk.line); g.drawRoundedRectangle(cell.reduced(0.5f), 2.5f, 1.0f); }
                g.setColour(on ? sk.bg : sk.dim.brighter(0.2f));
                ui::fitText(g, optAt(q, i), cell.reduced(3.0f, 0.0f), ui::labelBold(9.0f));
            }
            break;
        }
        case menus::Slider: {
            auto tr = r.withSizeKeepingCentre(r.getWidth(), th);
            g.setColour(sk.panel2);
            g.fillRoundedRectangle(tr, 2.5f);
            const float t = q.n > 1 ? frac(q, unitsAt(q, q.cur)) : 0.0f;
            float from = 0.0f;
            if (q.off >= 0) {                    // the OFF stretch: hatched, its edge a soft shadow
                const float o = unitsAt(q, q.off);
                const float x0 = tr.getX() + tr.getWidth() * frac(q, o - q.offW * 0.5f), x1 = tr.getX() + tr.getWidth() * frac(q, o + q.offW * 0.5f);
                ui::hatch(g, { x0, tr.getY(), x1 - x0, tr.getHeight() });
                from = frac(q, o);
            }
            if (q.centre >= 0) {                 // both ways: its middle marked, filled from it
                from = frac(q, unitsAt(q, q.centre));
                g.setColour(sk.line.brighter(0.3f));
                g.fillRect(tr.getX() + tr.getWidth() * from - 0.5f, tr.getY(), 1.0f, tr.getHeight());
            }
            const bool isOff = q.off >= 0 && q.cur == q.off;
            if (! isOff) {
                g.setColour(tint.withAlpha(hover || dragP == &q ? 0.5f : 0.34f));
                g.fillRect(juce::Rectangle<float>(tr.getX() + tr.getWidth() * juce::jmin(from, t), tr.getY() + 1.0f, tr.getWidth() * std::abs(t - from), tr.getHeight() - 2.0f));
            }
            if (q.n <= 33)                       // (few steps: each one marked)
                for (int i = 1; i < q.n - 1; ++i) {
                    g.setColour(sk.line.withAlpha(0.8f));
                    g.fillRect(tr.getX() + tr.getWidth() * frac(q, unitsAt(q, i)), tr.getBottom() - 4.0f, 1.0f, 4.0f);
                }
            g.setColour(sk.hot);
            g.fillRect(tr.getX() + tr.getWidth() * t - 1.0f, tr.getY(), 2.0f, tr.getHeight());
            g.setColour(isOff ? sk.dim : sk.ink);
            ui::fitText(g, q.val, tr.reduced(8.0f, 0.0f), ui::value(16.0f), juce::Justification::centredRight);
            break;
        }
        case menus::List: {
            auto b = r.withSizeKeepingCentre(r.getWidth(), th);
            g.setColour(hover ? sk.panel2.brighter(0.08f) : sk.panel2);
            g.fillRoundedRectangle(b, 2.5f);
            g.setColour(sk.line);
            g.drawRoundedRectangle(b.reduced(0.5f), 2.5f, 1.0f);
            g.setColour(sk.ink);
            ui::fitText(g, q.val, b.reduced(10.0f, 0.0f).withTrimmedRight(18.0f), ui::value(16.0f), juce::Justification::centredLeft);
            juce::Path tri;
            const float cx = b.getRight() - 14.0f, cy = b.getCentreY();
            tri.addTriangle(cx - 4.0f, cy - 2.5f, cx + 4.0f, cy - 2.5f, cx, cy + 3.5f);
            g.setColour(sk.dim);
            g.fillPath(tri);
            break;
        }
        default: {                                // (not looked at yet, or one value): its value, < > if it moves
            auto b = r.withSizeKeepingCentre(r.getWidth(), th);
            if (q.kind == menus::Unknown) {
                ui::button(g, b.withWidth(26.0f), "<", false, false, 11.0f);
                ui::button(g, b.withLeft(b.getRight() - 26.0f), ">", false, false, 11.0f);
                b = b.reduced(30.0f, 0.0f);
            }
            g.setColour(sk.ink);
            ui::fitText(g, q.val, b, ui::value(16.0f));
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
    int optHit(const Param& q, juce::Point<float> p) const
    {
        if (q.tiles) { for (int i = 0; i < q.n; ++i) if (tileR(q, i).contains(p)) return i; }
        else if (q.listed) { for (int i = 0; i < q.n; ++i) if (listR(q, i).contains(p)) return i; }
        return -1;
    }
    void mouseMove(const juce::MouseEvent& e) override
    {
        auto h = hit(e.position);
        auto* q = at(h);
        const int o = q ? optHit(*q, e.position) : -1;
        if (h.first != hoverCard || h.second != hoverParam || o != hoverOpt) { hoverCard = h.first; hoverParam = h.second; hoverOpt = o; repaint(); }
        setMouseCursor(q && q->kind == menus::Slider && ! q->tiles && q->ctl.contains(e.position) ? (cardOf(q) && cardOf(q)->faders ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::LeftRightResizeCursor)
                       : q && (q->tiles || q->listed) && o >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit(const juce::MouseEvent&) override { hoverCard = hoverParam = hoverOpt = -1; repaint(); }
    // to value i: the steps from where it is (a list that goes round: the short way)
    void go(Param& q, int i)
    {
        if (q.fxPick) {                          // (the effect: PRESETS turned by the difference)
            i = juce::jlimit(0, q.n - 1, i);
            if (i != q.cur) ed.turn(LB_PRESET, i - q.cur);
            q.cur = i;
            repaint();
            return;
        }
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
    // a slider's places: each value a unit wide, its OFF wider (offW); where value i sits (0..1), the value at u units
    static float wOf(const Param& q, int i) { return i == q.off ? q.offW : 1.0f; }
    static float unitsAt(const Param& q, int i) { float u = 0.0f; for (int j = 0; j < i; ++j) u += wOf(q, j); return u + wOf(q, i) * 0.5f; }
    static float unitsAll(const Param& q) { return q.n > 1 ? unitsAt(q, q.n - 1) - unitsAt(q, 0) : 1.0f; }
    static float frac(const Param& q, float u) { return juce::jlimit(0.0f, 1.0f, (u - unitsAt(q, 0)) / unitsAll(q)); }
    static int indexAtUnits(const Param& q, float u)
    {
        int best = 0;
        float bd = 1e9f;
        for (int i = 0; i < q.n; ++i) { const float d = std::abs(unitsAt(q, i) - u); if (d < bd) { bd = d; best = i; } }
        return best;
    }
    void nudge(Param& q, int d)                  // a step, as a detent: stops at the ends (a slider, buttons)
    {
        if (q.kind == menus::Unknown) { ed.proc.setValue(q.part, q.knob, d); return; }
        go(q, q.cur + d);
    }
    // the EQ's points: the value of the gain (up / down) and of the frequency (sideways) nearest where the point is put
    void eqTo(const juce::String& name, float want, bool hz)
    {
        auto* q = findPm(name);
        if (! q || q->kind == menus::Unknown || q->n < 2) return;
        int best = q->cur;
        float bd = 1e9f;
        for (int i = 0; i < q->n; ++i) {
            const auto v = optAt(*q, i);
            const float x = hz ? std::log(juce::jmax(1.0f, parseHz(v))) : v.getFloatValue();
            const float d = std::abs(x - (hz ? std::log(juce::jmax(1.0f, want)) : want));
            if (d < bd) { bd = d; best = i; }
        }
        go(*q, best);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int ci = 0; ci < (int) cards.size(); ++ci)
            if (auto& c = cards[(size_t) ci]; c.eq && eqArea(c).contains(e.position)) {
                const auto a = eqArea(c);
                const auto pts = eqPoints();
                eqDrag = -1;
                float bd = 30.0f;
                for (int i = 0; i < 3; ++i) {
                    const float d = e.position.getDistanceFrom({ eqX(a, pts[(size_t) i].hz), eqY(a, pts[(size_t) i].db) });
                    if (d < bd) { bd = d; eqDrag = i; }
                }
                eqCardI = ci;
                repaint();
                return;
            }
        auto* q = at(hit(e.position));
        if (! q || ! q->ctl.contains(e.position)) return;
        if (e.mods.isPopupMenu() && q->n > 1) { listMenu(*q); return; }
        if (q->tiles || q->listed) { const int o = optHit(*q, e.position); if (o >= 0) go(*q, o); return; }
        switch (q->kind) {
        case menus::Toggle:
            if (q->n == 2) { go(*q, (int) ((e.position.x - q->ctl.getX()) / (q->ctl.getWidth() / 2))); }
            break;
        case menus::Segments: go(*q, (int) ((e.position.x - q->ctl.getX()) / (q->ctl.getWidth() / q->n))); break;
        case menus::List: listMenu(*q); break;
        case menus::Slider:
            dragP = q;
            dragKey = q->key;
            dragV = cardOf(q) != nullptr && cardOf(q)->faders;
            if (dragV) {                         // (a fader: clicked where it should be)
                const float t = juce::jlimit(0.0f, 1.0f, (q->ctl.getBottom() - e.position.y) / q->ctl.getHeight());
                go(*q, indexAtUnits(*q, unitsAt(*q, 0) + t * unitsAll(*q)));
            }
            dragFrom = dragSent = q->cur;
            dragU = unitsAt(*q, q->cur);
            // the steps' width: few steps wide, many narrow (the whole range over about the slider and a half)
            pxStep = juce::jlimit(2.5f, 28.0f, (dragV ? q->ctl.getHeight() : q->ctl.getWidth() * 1.4f) / unitsAll(*q));   // (a unit's pixels)
            break;
        case menus::Unknown:
            nudge(*q, e.position.x < q->ctl.getCentreX() ? -1 : 1);
            break;
        default: break;
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (eqDrag >= 0 && eqCardI >= 0 && eqCardI < (int) cards.size()) {
            const auto a = eqArea(cards[(size_t) eqCardI]);
            const auto pts = eqPoints();
            const float db = juce::jlimit(-12.0f, 12.0f, (a.getCentreY() - e.position.y) / (a.getHeight() * 0.5f - 6.0f) * 15.0f);
            const float hz = 20.0f * std::pow(1000.0f, juce::jlimit(0.0f, 1.0f, (e.position.x - a.getX()) / a.getWidth()));
            eqTo(pts[(size_t) eqDrag].gain, db, false);
            eqTo(pts[(size_t) eqDrag].freq, hz, true);
            repaint();
            return;
        }
        if (! dragP) return;
        const float dx = dragV ? (float) -e.getDistanceFromDragStartY()
                               : (float) e.getDistanceFromDragStartX() - (float) e.getDistanceFromDragStartY() * 0.5f;
        const float slow = e.mods.isShiftDown() ? 4.0f : 1.0f;
        const int want = juce::jlimit(0, dragP->n - 1, indexAtUnits(*dragP, dragU + dx / (pxStep * slow)));
        if (want != dragSent) { go(*dragP, want); dragSent = want; }
    }
    void mouseUp(const juce::MouseEvent&) override { dragP = nullptr; dragKey.clear(); eqDrag = -1; eqCardI = -1; repaint(); }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        auto* q = at(hit(e.position));
        if (q && q->kind == menus::Slider && ! q->tiles && q->ctl.contains(e.position)) {
            if (q->off >= 0) go(*q, q->off);     // (a double click: back to OFF, or to the middle; every value: a right click)
            else if (q->centre >= 0) go(*q, q->centre);
            else listMenu(*q);
        }
    }
    // (FLEXOR: "pas de modification des sliders avec la molette": the wheel scrolls the cards, never a value)
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
                             .withMinimumWidth((int) juce::jmin(300.0f, q.ctl.getWidth())).withStandardItemHeight(22),
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
        content.availH = getHeight();
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
    juce::Colour laneCol(int r) const { return r == selRow ? s().hot : s().accent; }   // (the skin's, as the FM-1's SEQ)
    juce::Rectangle<float> gridArea() const { return { HEAD_W + 8.0f, BANK_H + RULER_H + 8.0f, getWidth() - HEAD_W - 16.0f, rowsH() }; }
    // the lanes as tall as the room allows (FLEXOR: the sequencer on the whole height), the inspector 120 px under them
    float rowH(int r) const
    {
        const float base = r == 0 ? 84.0f : r == 1 ? 60.0f : 52.0f;
        const float avail = (float) getHeight() - (BANK_H + RULER_H + 8.0f) - 126.0f - 5 * 4.0f;
        return base * juce::jlimit(1.0f, 2.2f, avail / 300.0f);
    }
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
    // a drum's step, as the FM-1's SEQ draws it in its skin (seq.c seq_picture): empty, the skin's block (every fourth
    // its line); on, its secondary colour (the lane edited: its vivid one); the step playing, the skin's white; its HITS
    // split it, a CHANCE darkens its lower half, a NUDGE a white mark how late, a kick's note a white line on top
    void drawDrumStep(juce::Graphics& g, int r, juce::Rectangle<float> c, int i, bool echo, bool sel, bool now)
    {
        const auto& sk = s();
        const bool o = on(r, i);
        const uint8_t b = sb(r, i);
        const float fade = echo ? 0.35f : 1.0f;
        juce::Colour col = o ? (r == selRow ? sk.hot : sk.accent) : (i % 4 == 0 ? sk.line : sk.panel2);
        if (now) col = sk.white;
        g.setColour(col.withMultipliedAlpha(fade));
        g.fillRect(c);
        if (now)                                  // (the white frame, the step's own inside)
            g.setColour((o ? (r == selRow ? sk.hot : sk.accent) : sk.panel2).withMultipliedAlpha(fade)), g.fillRect(c.reduced(3.0f));
        if (o && ! echo) {
            const int hits = (b & 3) + 1;
            for (int k = 1; k < hits; ++k) {      // HITS: the step split
                g.setColour(sk.bg);
                g.fillRect(c.getX() + c.getWidth() * k / hits, c.getY() + 2.0f, 1.5f, c.getHeight() - 4.0f);
            }
            if ((b >> 2 & 7) && (b >> 2 & 7) < 6) {   // CHANCE: its lower half the block's
                g.setColour(sk.panel2.withAlpha(0.85f));
                g.fillRect(c.withTrimmedTop(c.getHeight() * 0.5f).reduced(1.0f, 1.0f));
                g.setColour(sk.bg);
                g.setFont(ui::value(10.0f));
                static const char* CH[6] = { "", "75", "50", "25", "1:2", "2:2" };
                g.drawText(CH[b >> 2 & 7], c.withTrimmedTop(c.getHeight() * 0.5f), juce::Justification::centred);
            }
            if (b >> 5) {                         // NUDGE: a white mark, how late
                g.setColour(sk.white);
                g.fillRect(c.getX() + (c.getWidth() - 4.0f) * (b >> 5) / 8.0f, c.getBottom() - 4.0f, 4.0f, 3.0f);
            }
            if (r == 1 && ed.seq.note[i]) {       // the kick's note: a white line on top, its value
                g.setColour(sk.white);
                g.fillRect(c.getX() + 1.0f, c.getY(), c.getWidth() - 2.0f, 3.0f);
                g.setColour(sk.bg);
                g.setFont(ui::value(11.0f));
                g.drawText((ed.seq.note[i] > 0 ? "+" : "") + juce::String(ed.seq.note[i]), c.withTrimmedTop(4.0f).withHeight(14.0f), juce::Justification::centred);
            }
        }
        if (sel) { g.setColour(sk.hot); g.drawRect(c.expanded(2.0f), 2.0f); }
    }
    // a break's step: its slice's number in the skin's vivid colour (LOOP "." and OFF "-" its labels'), reversed or
    // repeated a mark in its secondary; the step playing in the vivid one, its number dark (seq.c)
    void drawBreakStep(juce::Graphics& g, juce::Rectangle<float> c, int i, bool echo, bool sel, bool now)
    {
        const auto& sk = s();
        const uint8_t* b = ed.seq.brk[i & 31];
        const float fade = echo ? 0.4f : 1.0f;
        g.setColour((now ? sk.hot : i % 4 == 0 ? sk.line : sk.panel2).withMultipliedAlpha(fade));
        g.fillRect(c);
        // its sound: the slice it plays (LOOP: the break where it is), as a waveform
        const auto& w = ed.wave;
        const int n = (int) w.cols.size() / 4;
        float a0 = -1.0f, a1 = -1.0f;
        if (b[0] == 1) { a0 = i / 32.0f; a1 = (i + 1) / 32.0f; }
        else if (b[0] >= 2 && ! w.slices.empty()) {
            const int sl = juce::jlimit(0, (int) w.slices.size() - 1, b[0] - 2);
            a0 = w.slices[(size_t) sl];
            a1 = sl + 1 < (int) w.slices.size() ? w.slices[(size_t) sl + 1] : 1.0f;
        }
        if (n > 0 && a0 >= 0.0f) {
            const float mid = c.getCentreY() + 6.0f, half = c.getHeight() * 0.33f;
            const bool rev = b[2] & 1;
            for (float x = 0; x < c.getWidth(); x += 2.0f) {
                float t = x / c.getWidth();
                if (rev) t = 1.0f - t;
                const int col = juce::jlimit(0, n - 1, (int) ((a0 + (a1 - a0) * t) * n));
                const float pk = w.cols[(size_t) (4 * col)];
                g.setColour((now ? sk.bg : b[0] == 1 ? ui::rgb(w.cols[(size_t) (4 * col + 1)], w.cols[(size_t) (4 * col + 2)], w.cols[(size_t) (4 * col + 3)], 0.4f) : ui::rgb(w.cols[(size_t) (4 * col + 1)], w.cols[(size_t) (4 * col + 2)], w.cols[(size_t) (4 * col + 3)])).withMultipliedAlpha(fade));
                g.fillRect(c.getX() + x, mid - pk * half, 1.5f, juce::jmax(1.0f, 2.0f * pk * half));
            }
        }
        g.setColour(now ? sk.bg : b[0] >= 2 ? sk.hot : sk.dim);
        g.setFont(ui::value(13.0f));
        g.drawText(b[0] == 0 ? "-" : b[0] == 1 ? "." : juce::String(b[0] - 1), c.withHeight(16.0f), juce::Justification::centred);
        if ((b[2] & 7) && ! echo) {              // reversed or repeated: a mark
            g.setColour(now ? sk.bg : sk.accent);
            g.fillRect(c.getX() + 2.0f, c.getBottom() - 3.0f, c.getWidth() - 4.0f, 2.0f);
        }
        juce::String tags;
        if ((int8_t) b[1]) tags << ((int8_t) b[1] > 0 ? "+" : "") << (int) (int8_t) b[1];
        if (b[2] >> 1 & 3) tags << " x" << ((b[2] >> 1 & 3) + 1);
        if (b[2] & 1) tags << " R";
        if (tags.isNotEmpty()) {
            g.setColour(now ? sk.bg : sk.ink);
            ui::fitText(g, tags.trim(), c.withTrimmedTop(c.getHeight() - 16.0f).withTrimmedBottom(3.0f), ui::label(9.5f));
        }
        if (sel) { g.setColour(sk.hot); g.drawRect(c.expanded(2.0f), 2.0f); }
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
    // the keyboard (FLEXOR: "se déplacer facilement de gauche à droite et de haut en bas dans le séquenceur et appuyer
    // sur entrée pour mettre une step"): the cursor (the step picked), the step on / off, its values, the lane moved
    void command(int c)
    {
        using A = KeyAction;
        const int len = laneLen(selRow);
        auto col = [&](int v) { selCol = juce::jlimit(0, laneLen(selRow) - 1, v); };
        const uint8_t* b = ed.seq.brk[selCol & 31];
        switch (c) {
        case A::SeqLeft: col(selCol - 1); break;
        case A::SeqRight: col(selCol + 1); break;
        case A::SeqBeatLeft: col(selCol % 4 ? selCol / 4 * 4 : selCol - 4); break;
        case A::SeqBeatRight: col((selCol / 4 + 1) * 4); break;
        case A::SeqBarLeft: col(selCol % 16 ? selCol / 16 * 16 : selCol - 16); break;
        case A::SeqBarRight: col((selCol / 16 + 1) * 16); break;
        case A::SeqHome: col(0); break;
        case A::SeqEnd: col(len - 1); break;
        case A::SeqUp: selRow = juce::jmax(0, selRow - 1); col(selCol); break;
        case A::SeqDown: selRow = juce::jmin(4, selRow + 1); col(selCol); break;
        case A::SeqToggle:
            if (selRow > 0) { ed.proc.step(1, selRow - 1, selCol, 0, 0); ed.seq.mask[selRow - 1][selCol >> 4] ^= (uint16_t) (1u << (selCol & 15)); }
            else ed.proc.step(4, 0, selCol & 31, 0, b[0] ? -(int) b[0] : 1);   // (the break: OFF, or LOOP)
            break;
        case A::SeqClear:
            if (selRow > 0 && on(selRow, selCol)) { ed.proc.step(1, selRow - 1, selCol, 0, 0); ed.seq.mask[selRow - 1][selCol >> 4] ^= (uint16_t) (1u << (selCol & 15)); }
            else if (selRow == 0 && b[0]) ed.proc.step(4, 0, selCol & 31, 0, -(int) b[0]);
            break;
        case A::SeqMainUp: case A::SeqMainDown: {
            const int d = c == A::SeqMainUp ? 1 : -1;
            if (selRow == 0) ed.proc.step(4, 0, selCol & 31, 0, d);           // (the break: its slice)
            else ed.proc.step(2, selRow - 1, selCol, 1, d);                     // (a drum: its HITS)
            break;
        }
        case A::SeqAltUp: case A::SeqAltDown: {
            const int d = c == A::SeqAltUp ? 1 : -1;
            if (selRow == 0) ed.proc.step(4, 0, selCol & 31, 1, d);           // (the break: its pitch)
            else ed.proc.step(2, selRow - 1, selCol, selRow == 1 ? 0 : 2, d);   // (the kick: its note; a drum: its chance)
            break;
        }
        case A::SeqChanceUp: case A::SeqChanceDown: {
            const int d = c == A::SeqChanceUp ? 1 : -1;
            if (selRow == 0) ed.proc.step(4, 0, selCol & 31, 2, d);           // (the break: its repeat)
            else ed.proc.step(2, selRow - 1, selCol, 2, d);
            break;
        }
        case A::SeqNudgeUp: case A::SeqNudgeDown: {
            const int d = c == A::SeqNudgeUp ? 1 : -1;
            if (selRow == 0) ed.proc.step(4, 0, selCol & 31, 3, d);           // (the break: reversed or not)
            else ed.proc.step(2, selRow - 1, selCol, 3, d);
            break;
        }
        case A::SeqRotLeft: ed.proc.step(9, selRow == 0 ? 4 : selRow - 1, 0, 0, -1); break;
        case A::SeqRotRight: ed.proc.step(9, selRow == 0 ? 4 : selRow - 1, 0, 0, 1); break;
        default: break;
        }
        repaint();
    }
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


// ------------------------------------------------------------------------------------------------ the arranger ---
// (FLEXOR: "penser la VST comme un mini DAW"): SONG's 8 tracks on a timeline, each item a block as long as it plays
// (a pattern's loops, a drop's build), dragged to its place, + to add, its pattern, loops, break and kick below; the
// track played from here. Written whole through the firmware's own check (lbcore lb_track_write)
struct SongPanel : Part
{
    explicit SongPanel(LBE& e) : Part(e) {}
    std::vector<lb_track_t> tr;
    int selT = 0, selI = -1, dragI = -1, dragT = -1, hover = -1;
    float dragX = 0.0f, downX = 0.0f;
    static constexpr float HEAD = 170.0f, LANE_H = 34.0f, TOP = 28.0f;
    juce::Rectangle<float> lane(int t) const { return { 8.0f, TOP + t * (LANE_H + 4.0f), (float) getWidth() - 16.0f, LANE_H }; }
    juce::Rectangle<float> inspector() const { const float y = TOP + 8 * (LANE_H + 4.0f) + 6.0f; return { 8.0f, y, (float) getWidth() - 16.0f, (float) getHeight() - y - 6.0f }; }
    static int count(const lb_track_t& t) { int n = 0; while (n < 32 && t.it[n]) n++; return n; }
    static int bars(const lb_track_t& t, int i) { return t.it[i] == 0xFE ? juce::jmax(1, t.dropbars) : juce::jmax(1, (int) t.rep[i]) * juce::jmax(1, t.patbars); }
    float pxBar() const
    {
        int most = 32;
        for (auto& t : tr) { int b = 0; for (int i = 0; i < count(t); ++i) b += bars(t, i); most = juce::jmax(most, b + 4); }
        return ((float) getWidth() - 16.0f - HEAD - 40.0f) / (float) most;
    }
    juce::Rectangle<float> block(int t, int i) const
    {
        const auto l = lane(t);
        float x = l.getX() + HEAD;
        for (int k = 0; k < i; ++k) x += bars(tr[(size_t) t], k) * pxBar();
        return { x + 1.0f, l.getY() + 2.0f, bars(tr[(size_t) t], i) * pxBar() - 2.0f, l.getHeight() - 4.0f };
    }
    juce::Rectangle<float> plus(int t) const
    {
        const int n = count(tr[(size_t) t]);
        const float x = n ? block(t, n - 1).getRight() + 4.0f : lane(t).getX() + HEAD + 2.0f;
        return { x, lane(t).getY() + 5.0f, 26.0f, LANE_H - 10.0f };
    }
    juce::Rectangle<float> playBtn(int t) const { return { lane(t).getX() + 96.0f, lane(t).getY() + 5.0f, 30.0f, LANE_H - 10.0f }; }
    juce::Rectangle<float> endBtn(int t) const { return { lane(t).getX() + 130.0f, lane(t).getY() + 5.0f, 34.0f, LANE_H - 10.0f }; }
    juce::String label(const lb_track_t& t, int i) const
    {
        const int v = t.it[i];
        if (v == 0xFE) return "DROP " + juce::String((t.tb[i] >> 3 & 15) ? juce::String((t.tb[i] >> 3) & 15) : juce::String());
        if (v == 0xFD) return "GAP";
        return "P" + juce::String(v) + (t.rep[i] > 1 ? "  x" + juce::String(t.rep[i]) : juce::String());
    }
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        g.setColour(sk.bg.brighter(0.02f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
        g.setColour(sk.line);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
        g.setColour(sk.dim);
        g.setFont(ui::label(8.5f).withExtraKerningFactor(0.14f));
        g.drawText("TRACKS  /  CLIC : CHOISIR   GLISSER : DEPLACER   DOUBLE-CLIC : LE PATTERN   + : AJOUTER   > : JOUER", 14, 4, 1000, 20, juce::Justification::centredLeft);
        if (tr.size() < 8) return;
        const float pb = pxBar();
        for (int t = 0; t < 8; ++t) {
            const auto& k = tr[(size_t) t];
            auto l = lane(t);
            const bool cur = k.cur == t;
            g.setColour(cur ? sk.panel2.brighter(0.04f) : sk.panel);
            g.fillRoundedRectangle(l, 4.0f);
            if (cur) { g.setColour(sk.ink); g.fillRoundedRectangle(l.withWidth(3.0f), 1.5f); }
            for (int b = 0; b * pb < l.getWidth() - HEAD; b += 4) {   // (the bars: every 4 a line)
                g.setColour(sk.line.withAlpha(b % 16 ? 0.35f : 0.8f));
                g.fillRect(l.getX() + HEAD + b * pb, l.getY(), 1.0f, l.getHeight());
            }
            g.setColour(cur ? sk.hot : sk.ink);
            g.setFont(ui::labelBold(14.0f));
            g.drawText("TRACK " + juce::String(t + 1), l.withWidth(92.0f).translated(8.0f, 0.0f), juce::Justification::centredLeft);
            ui::button(g, playBtn(t), k.on && cur ? "[]" : ">", k.on && cur, false, 12.0f);
            static const char* END[3] = { "LOOP", "STOP", "HOLD" };
            ui::button(g, endBtn(t), END[juce::jlimit(0, 2, k.end)], false, false, 9.5f);
            const int n = count(k);
            for (int i = 0; i < n; ++i) {
                if (t == dragT && i == dragI) continue;
                drawBlock(g, t, i, block(t, i));
            }
            if (t == dragT && dragI >= 0) drawBlock(g, t, dragI, block(t, dragI).translated(dragX, 0.0f).withY(l.getY() - 2.0f));
            if (n < 32) ui::button(g, plus(t), "+", false, false, 13.0f);
        }
        drawInspector(g);
    }
    void drawBlock(juce::Graphics& g, int t, int i, juce::Rectangle<float> r)
    {
        const auto& sk = s();
        const auto& k = tr[(size_t) t];
        const int v = k.it[i];
        const bool sel = t == selT && i == selI, now = k.on && k.cur == t && k.pos == i;
        // a pattern: its group's colour on its left (P1-16 red, 17-32 amber, 33-48 green, 49-64 blue), its kicks and
        // snares drawn small, bar after bar; a drop bright; a gap hatched
        static const char* GRP[4] = { "SUB", "PUNCH", "GRIT", "CLICK" };
        const bool pat = v >= 1 && v <= 64;
        const auto gc = pat ? ui::layer(GRP[(v - 1) / 16]) : sk.hot;
        g.setColour(v == 0xFE ? sk.hot.withAlpha(0.85f) : v == 0xFD ? sk.panel2 : sk.panel2.brighter(0.1f));
        g.fillRoundedRectangle(r, 3.0f);
        if (v == 0xFD) ui::hatch(g, r);
        if (pat) {
            g.setColour(gc);
            g.fillRoundedRectangle(r.withWidth(4.0f), 1.5f);
            const uint8_t* pp = ed.pats + (size_t) (v - 1) * 40;
            const float pb = pxBar(), stepW = pb / 16.0f;
            const int nb = (int) (r.getWidth() / juce::jmax(1.0f, pb));
            for (int b = 0; b < nb && stepW > 0.8f; ++b)
                for (int d = 0; d < 3; ++d) {            // (KICK, SNARE, HAT: three rows of ticks)
                    const int bars = juce::jmax(1, (int) pp[d]);
                    const uint16_t m = (uint16_t) (pp[4 + d * 8 + (b % bars) * 2] | pp[4 + d * 8 + (b % bars) * 2 + 1] << 8);
                    for (int st = 0; st < 16; ++st)
                        if (m >> st & 1u) {
                            g.setColour((d == 0 ? gc : sk.ink).withAlpha(d == 0 ? 0.9f : 0.45f));
                            g.fillRect(r.getX() + 6.0f + b * pb + st * stepW, r.getBottom() - 5.0f - d * 4.0f, juce::jmax(1.0f, stepW - 0.5f), 2.5f);
                        }
                }
        }
        if (now) { g.setColour(sk.white); g.drawRoundedRectangle(r.expanded(1.0f), 3.0f, 2.0f); }
        if (sel) { g.setColour(sk.hot); g.drawRoundedRectangle(r.expanded(2.0f), 3.0f, 2.0f); }
        g.setColour(v == 0xFD ? sk.dim : v == 0xFE ? sk.bg : sk.ink);
        ui::fitText(g, label(k, i), r.reduced(8.0f, 0.0f).withTrimmedBottom(r.getHeight() * 0.42f), ui::labelBold(11.0f), juce::Justification::centredLeft);
        juce::String sub;
        if (v != 0xFE && v != 0xFD && k.tb[i]) sub << "B" << (int) k.tb[i] << " ";
        if (k.tk[i]) sub << "K" << (int) k.tk[i];
        if (sub.isNotEmpty()) { g.setColour(sk.dim); ui::fitText(g, sub, r.reduced(8.0f, 0.0f).withTrimmedRight(r.getWidth() * 0.4f).withTrimmedTop(r.getHeight() * 0.5f).withTrimmedBottom(8.0f), ui::label(8.0f), juce::Justification::centredLeft); }
    }
    // the item picked: its pattern, loops, break, kick; out, a copy, its kind
    std::vector<std::pair<juce::Rectangle<float>, int>> ib;
    void drawInspector(juce::Graphics& g)
    {
        const auto& sk = s();
        ib.clear();
        auto a = inspector();
        if (selI < 0 || selT >= (int) tr.size() || selI >= count(tr[(size_t) selT])) {
            g.setColour(sk.dim);
            g.setFont(ui::label(12.0f));
            g.drawText("Clique un element d'une track pour le regler ici.", a, juce::Justification::centredLeft);
            return;
        }
        const auto& k = tr[(size_t) selT];
        g.setColour(sk.ink);
        g.setFont(ui::labelBold(14.0f));
        g.drawText("TRACK " + juce::String(selT + 1) + "  /  " + juce::String(selI + 1) + ":  " + label(k, selI), a.removeFromLeft(240.0f), juce::Justification::centredLeft);
        auto bt = [&](const juce::String& t, int id, float w) { auto r = a.removeFromLeft(w).reduced(2.0f, 4.0f); ui::button(g, r, t, false, false, 11.0f); ib.push_back({ r, id }); a.removeFromLeft(4.0f); };
        const bool pat = k.it[selI] != 0xFE && k.it[selI] != 0xFD;
        if (pat) { bt("< P", 1, 34); bt("PATTERN...", 2, 92); bt("P >", 3, 34); bt("- LOOP", 4, 62); bt("LOOP +", 5, 62); bt("BREAK...", 6, 84); }
        bt("KICK...", 7, 76);
        bt("<< ", 8, 40); bt(" >>", 9, 40);
        bt("DUPLIQUER", 10, 92); bt("SUPPRIMER", 11, 92);
    }
    void write(const lb_track_t& t) { ed.proc.trackWrite(t); repaint(); }
    void popPattern(int t, int i, bool insert)
    {
        juce::PopupMenu m;
        for (int gq = 0; gq < 4; ++gq) {
            juce::PopupMenu sub;
            for (int p = gq * 16 + 1; p <= gq * 16 + 16; ++p) sub.addItem(p, "P" + juce::String(p));
            m.addSubMenu("P" + juce::String(gq * 16 + 1) + " - P" + juce::String(gq * 16 + 16), sub);
        }
        m.addSeparator();
        m.addItem(0xFE + 1000, "DROP");
        m.addItem(0xFD + 1000, "GAP (un trou a remplir)");
        juce::Component::SafePointer<SongPanel> self(this);
        m.showMenuAsync(juce::PopupMenu::Options(), [self, t, i, insert](int r) {
            if (! self || r == 0 || t >= (int) self->tr.size()) return;
            auto k = self->tr[(size_t) t];
            const int v = r >= 1000 ? r - 1000 : r, n = count(k);
            if (insert) {
                if (n >= 32) return;
                k.it[n] = (uint8_t) v; k.rep[n] = 1; k.tb[n] = 0; k.tk[n] = 0;
                self->selT = t; self->selI = n;
            } else
                k.it[i] = (uint8_t) v;
            self->tr[(size_t) t] = k;
            self->write(k);
        });
    }
    void listMenu(const juce::StringArray& items, int current, std::function<void(int)> done)
    {
        juce::PopupMenu m;
        for (int i = 0; i < items.size(); ++i) m.addItem(1 + i, items[i], true, i == current);
        m.showMenuAsync(juce::PopupMenu::Options(), [done](int r) { if (r > 0) done(r - 1); });
    }
    void inspectorClick(int id)
    {
        if (selT >= (int) tr.size()) return;
        auto k = tr[(size_t) selT];
        const int n = count(k);
        if (selI < 0 || selI >= n) return;
        juce::Component::SafePointer<SongPanel> self(this);
        const int t = selT, i = selI;
        auto apply = [self, t](const lb_track_t& kk) { if (self) { self->tr[(size_t) t] = kk; self->write(kk); } };
        switch (id) {
        case 1: case 3: k.it[i] = (uint8_t) juce::jlimit(1, 64, (int) k.it[i] + (id == 3 ? 1 : -1)); apply(k); break;
        case 2: popPattern(t, i, false); break;
        case 4: case 5: k.rep[i] = (uint8_t) juce::jlimit(1, 16, (int) k.rep[i] + (id == 5 ? 1 : -1)); apply(k); break;
        case 6: {
            const auto all = ed.proc.breaks();
            juce::StringArray names { "LE BREAK EN JEU" };
            for (auto& b : all) names.add(b.name);
            listMenu(names, k.tb[i], [self, t, i, k](int c) mutable { if (! self) return; k.tb[i] = (uint8_t) c; self->tr[(size_t) t] = k; self->write(k); });
            break;
        }
        case 7: {
            juce::StringArray names { "LE KICK EN JEU" };
            for (int q = 1; q <= 20; ++q) names.add("KICK " + juce::String(q));
            listMenu(names, k.tk[i], [self, t, i, k](int c) mutable { if (! self) return; k.tk[i] = (uint8_t) c; self->tr[(size_t) t] = k; self->write(k); });
            break;
        }
        case 8: case 9: {                         // (moved a place earlier / later)
            const int j = id == 8 ? i - 1 : i + 1;
            if (j < 0 || j >= n) return;
            std::swap(k.it[i], k.it[j]); std::swap(k.rep[i], k.rep[j]); std::swap(k.tb[i], k.tb[j]); std::swap(k.tk[i], k.tk[j]);
            selI = j;
            apply(k);
            break;
        }
        case 10: {                                // (a copy after it)
            if (n >= 32) return;
            for (int q = n; q > i + 1; --q) { k.it[q] = k.it[q - 1]; k.rep[q] = k.rep[q - 1]; k.tb[q] = k.tb[q - 1]; k.tk[q] = k.tk[q - 1]; }
            k.it[i + 1] = k.it[i]; k.rep[i + 1] = k.rep[i]; k.tb[i + 1] = k.tb[i]; k.tk[i + 1] = k.tk[i];
            selI = i + 1;
            apply(k);
            break;
        }
        case 11: {                                // (out, the rest closing up)
            for (int q = i; q < 31; ++q) { k.it[q] = k.it[q + 1]; k.rep[q] = k.rep[q + 1]; k.tb[q] = k.tb[q + 1]; k.tk[q] = k.tk[q + 1]; }
            k.it[31] = 0;
            selI = juce::jmin(i, count(k) - 1);
            apply(k);
            break;
        }
        default: break;
        }
    }
    std::pair<int, int> at(juce::Point<float> p) const
    {
        for (int t = 0; t < (int) tr.size(); ++t)
            for (int i = 0; i < count(tr[(size_t) t]); ++i)
                if (block(t, i).contains(p)) return { t, i };
        return { -1, -1 };
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        for (auto& b : ib) if (b.first.contains(p)) { inspectorClick(b.second); return; }
        if (tr.size() < 8) return;
        for (int t = 0; t < 8; ++t) {
            if (playBtn(t).contains(p)) { const bool on = tr[(size_t) t].on && tr[(size_t) t].cur == t; ed.proc.trackPlay(on ? 2 : 1, t); return; }
            if (endBtn(t).contains(p)) { auto k = tr[(size_t) t]; k.end = (k.end + 1) % 3; tr[(size_t) t] = k; write(k); return; }
            if (count(tr[(size_t) t]) < 32 && plus(t).contains(p)) { popPattern(t, 0, true); return; }
            if (lane(t).withWidth(HEAD).contains(p)) { ed.proc.trackPlay(3, t); selT = t; selI = -1; repaint(); return; }
        }
        const auto h = at(p);
        if (h.first >= 0) { selT = h.first; selI = h.second; dragT = h.first; dragI = h.second; dragX = 0.0f; downX = p.x; repaint(); }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragI < 0) return;
        dragX = e.position.x - downX;
        repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (dragI >= 0 && std::abs(dragX) > 6.0f && dragT < (int) tr.size()) {   // (dropped: before the block it lands on)
            auto k = tr[(size_t) dragT];
            const int n = count(k);
            const float x = block(dragT, dragI).getCentreX() + dragX;
            int to = n - 1;
            for (int i = 0; i < n; ++i) if (x < block(dragT, i).getCentreX()) { to = i > dragI ? i - 1 : i; break; }
            to = juce::jlimit(0, n - 1, to);
            if (to != dragI) {
                const uint8_t a[4] = { k.it[dragI], k.rep[dragI], k.tb[dragI], k.tk[dragI] };
                if (to < dragI) for (int q = dragI; q > to; --q) { k.it[q] = k.it[q - 1]; k.rep[q] = k.rep[q - 1]; k.tb[q] = k.tb[q - 1]; k.tk[q] = k.tk[q - 1]; }
                else for (int q = dragI; q < to; ++q) { k.it[q] = k.it[q + 1]; k.rep[q] = k.rep[q + 1]; k.tb[q] = k.tb[q + 1]; k.tk[q] = k.tk[q + 1]; }
                k.it[to] = a[0]; k.rep[to] = a[1]; k.tb[to] = a[2]; k.tk[to] = a[3];
                tr[(size_t) dragT] = k;
                selI = to;
                write(k);
            }
        }
        (void) e;
        dragI = dragT = -1;
        dragX = 0.0f;
        repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const auto h = at(e.position);
        if (h.first >= 0) popPattern(h.first, h.second, false);
    }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override   // (a block's loops)
    {
        const auto h = at(e.position);
        if (h.first < 0 || w.deltaY == 0.0f) return;
        auto k = tr[(size_t) h.first];
        if (k.it[h.second] == 0xFE || k.it[h.second] == 0xFD) return;
        k.rep[h.second] = (uint8_t) juce::jlimit(1, 16, (int) k.rep[h.second] + (w.deltaY > 0 ? 1 : -1));
        tr[(size_t) h.first] = k;
        selT = h.first; selI = h.second;
        write(k);
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
        g.drawRect(getWidth() - 39, 6, 11, 9, 1);   // (its own window)
        g.fillRect(getWidth() - 39, 6, 11, 3);
        auto r = getLocalBounds().toFloat().withTrimmedTop(22.0f).reduced(1.0f);
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImage(screen, r, juce::RectanglePlacement::centred);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.y < 22 && e.x > getWidth() - 22) { ed.toggleMachine(); return; }
        if (e.y < 22 && e.x > getWidth() - 44) { ed.machineSeparate(true); return; }
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
// the 12 black keys (and REC) and the 16 slices: folded to a strip under the menus (the menus' room), or open, big
struct Pads : Part
{
    explicit Pads(LBE& e) : Part(e) {}
    int held = -1;
    bool compact() const { return ! ed.padsOpen; }
    // (the FM-1 as the controller: the keys its hands hold now, lit too; plat.h's PLAT_WHITE_KEYS / PLAT_BLACK_KEYS)
    bool fm1White(int i) const { static const int W[16] = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26 }; return (ed.proc.fm1Keys() >> W[i] & 1u) != 0; }
    bool fm1Black(int i) const
    {
        static const int B[11] = { 1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25 };
        return i < 11 ? (ed.proc.fm1Keys() >> B[i] & 1u) != 0 : (ed.proc.fm1Buttons() >> LB_REC & 1u) != 0;
    }
    juce::Rectangle<float> fold() const { return compact() ? juce::Rectangle<float>(6, 8, 92, 32) : juce::Rectangle<float>((float) getWidth() - 110.0f, 4, 96, 18); }
    juce::Rectangle<float> white(int i) const
    {
        if (compact()) { const float x0 = 108.0f + 12 * 50.0f + 14.0f, w = ((float) getWidth() - x0 - 8.0f) / 16.0f; return { x0 + i * w, 9.0f, w - 4.0f, 30.0f }; }
        const float w = ((float) getWidth() - 40.0f) / 16.0f; return { 20.0f + i * w, 108.0f, w - 6.0f, 70.0f };
    }
    juce::Rectangle<float> black(int i) const
    {
        if (compact()) return { 108.0f + i * 50.0f, 9.0f, 46.0f, 30.0f };
        const float w = ((float) getWidth() - 40.0f) / 12.0f; return { 20.0f + i * w, 24.0f, w - 6.0f, 64.0f };
    }
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        auto& km = KeyMap::get();
        if (compact()) {
            g.setColour(sk.bg.brighter(0.025f));
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
            g.setColour(sk.line.withAlpha(0.7f));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
            g.setColour(sk.dim);
            g.setFont(ui::labelBold(9.0f).withExtraKerningFactor(0.14f));
            g.drawText(juce::CharPointer_UTF8("PADS  \xe2\x96\xb4"), fold(), juce::Justification::centred);
            for (int i = 0; i < 12; ++i) {
                auto r = black(i);
                const bool on = ed.m.padon[i] || held == 100 + i || fm1Black(i);
                g.setColour(on ? sk.accent : sk.panel2);
                g.fillRoundedRectangle(r, 2.0f);
                g.setColour(on ? sk.bg : sk.ink.withAlpha(0.8f));
                ui::fitText(g, ed.m.pad[i], r.reduced(3, 0), ui::labelBold(9.5f));
            }
            for (int i = 0; i < 16; ++i) {
                auto r = white(i);
                const bool on = ed.m.key == i || held == i || (ed.m.slice == i && ed.m.playing) || fm1White(i);
                g.setColour(on ? sk.hot : sk.panel2.brighter(0.03f));
                g.fillRoundedRectangle(r, 2.0f);
                g.setColour(on ? sk.bg : sk.dim);
                g.setFont(ui::labelBold(9.5f));
                g.drawText(juce::String(i + 1), r, juce::Justification::centred);
            }
            return;
        }
        ui::panel(g, getLocalBounds().toFloat().reduced(1.0f), "PADS  /  TOUCHES");
        g.setColour(sk.dim);
        g.setFont(ui::labelBold(8.5f));
        g.drawText(juce::CharPointer_UTF8("REPLIER  \xe2\x96\xbe"), fold(), juce::Justification::centredRight);
        for (int i = 0; i < 12; ++i) {
            auto r = black(i);
            const bool on = ed.m.padon[i] || held == 100 + i || fm1Black(i);
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
            const bool on = ed.m.key == i || held == i || (ed.m.slice == i && ed.m.playing) || fm1White(i);
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
        if (fold().contains(p)) return 999;
        for (int i = 0; i < 12; ++i) if (black(i).contains(p)) return 100 + i;
        for (int i = 0; i < 16; ++i) if (white(i).contains(p)) return i;
        return -1;
    }
    void press(int h, bool d)
    {
        if (h < 0 || h == 999) return;
        if (h >= 100) { if (h == 111) ed.hold(LB_REC, d); else ed.key(true, h - 100, d); }
        else ed.key(false, h, d);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        held = hit(e.position);
        if (held == 999) { held = -1; ed.togglePads(); return; }
        press(held, true);
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const int h = hit(e.position);
        if (h != held && h != 999) { press(held, false); held = h; press(held, true); repaint(); }
    }
    void mouseUp(const juce::MouseEvent&) override { press(held, false); held = -1; repaint(); }
};

// ------------------------------------------------------------------------- the FM-1's screen, its own window ---
// (FLEXOR: "pouvoir aussi mettre l'aperçu fm 1 dans une fenêtre séparée"): a window of the desktop, moved and sized
// as any (on another screen too), its place kept (settings "machineWin"); the keys go on to the editor's
struct MachineWindow : juce::DocumentWindow
{
    struct View : Part
    {
        View(LBE& e, juce::Image& im) : Part(e), screen(im) { setWantsKeyboardFocus(true); }
        juce::Image& screen;
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black);
            g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);   // (the pixels sharp)
            g.drawImage(screen, getLocalBounds().toFloat(), juce::RectanglePlacement::centred);
        }
        bool keyPressed(const juce::KeyPress& k) override { return ed.keyPressed(k); }
        bool keyStateChanged(bool d) override { return ed.keyStateChanged(d); }
    };
    MachineWindow(LBE& e, juce::Image& im) : juce::DocumentWindow("LOAF BREAKER - FM-1", juce::Colours::black,
                                                                   juce::DocumentWindow::closeButton), ed(e), view(e, im)
    {
        setUsingNativeTitleBar(true);
        setContentNonOwned(&view, false);
        setResizable(true, false);
        setResizeLimits(240, 240, 2400, 2400);
        if (auto* c = getConstrainer()) c->setFixedAspectRatio(1.0);
        auto b = juce::Rectangle<int>::fromString(ui::settings().getUserSettings()->getValue("machineWin"));
        const auto& ds = juce::Desktop::getInstance().getDisplays();
        if (b.getWidth() < 200 || ds.getDisplayForRect(b) == nullptr) {
            b = { 0, 0, 480, 480 };
            if (auto* d = ds.getPrimaryDisplay()) b = b.withPosition(d->userArea.getRight() - 520, d->userArea.getY() + 60);
        }
        setBounds(b);
        setVisible(true);
    }
    LBE& ed;
    View view;
    void closeButtonPressed() override { juce::MessageManager::callAsync([s = juce::Component::SafePointer<LBE>(&ed)] { if (s) s->toggleMachine(); }); }
    void moved() override { DocumentWindow::moved(); keep(); }
    void resized() override { DocumentWindow::resized(); keep(); }
    void keep() { if (isOnDesktop() && getWidth() > 100) ui::settings().getUserSettings()->setValue("machineWin", getBounds().toString()); }
};

// ---------------------------------------------------------------------------------------------------- root ---
struct LoafBreakerEditor::Root : Part
{
    explicit Root(LBE& e) : Part(e), header(e), cats(e), tabs(e), wave(e), hero(e), menus(e), seqp(e), song(e), machine(e), pads(e)
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &header, &cats, &tabs, &wave, &hero, &menus, &seqp, &song, &pads, &machine })
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
    SoundHero hero;
    MenuPage menus;
    SeqPanel seqp;
    SongPanel song;
    Machine machine;
    Pads pads;
    bool seqShown = false, songShown = false;
    int heroShown = -1;                  // (LBE::heroKind: -1 none, 0 the kick, 1..3 a drum, 9 the break)
    bool padsShown = false;
    // (FLEXOR, 25 Nov 2026: the mockup's A, "c'est la bonne direction"): the header, the domains, the tabs; the sound
    // drawn big (the kick, a drum, or the break); the page's parts as cards; the pads folded to a strip under them
    void resized() override
    {
        // (FLEXOR: "le plein écran n'est pas adapté à un écran 16:9 ... adapté à tous les écrans"): the window's own shape,
        // 1440 x 900 at least, the room past it given to the pictures and cards (wider) and to the menus (taller)
        const int w = getWidth(), h = getHeight();
        header.setBounds(0, 0, w, 86);
        cats.setBounds(0, 86, w, 44);
        tabs.setBounds(0, 130, w, 36);
        seqShown = ed.seqView();
        songShown = ed.songView();
        heroShown = ed.heroKind();
        padsShown = ed.padsOpen;
        ed.proc.tracksWanted = songShown;
        const int bottom = padsShown ? h - 214 : h - 64;     // (the menus' foot)
        pads.setBounds(8, padsShown ? h - 210 : h - 56, w - 16, padsShown ? 202 : 48);
        hero.what = heroShown >= 0 && heroShown <= 3 ? heroShown : 0;
        hero.setVisible(heroShown >= 0 && heroShown <= 3);
        wave.setVisible(heroShown == 9);
        hero.setBounds(8, 174, w - 16, 260);
        wave.setBounds(8, 174, w - 16, 260);
        seqp.setVisible(seqShown);
        song.setVisible(songShown);
        menus.setVisible(! seqShown);
        seqp.setBounds(8, 174, w - 16, bottom - 174);
        const int songH = juce::jmax(380, (bottom - 174) * 55 / 100);
        song.setBounds(8, 174, w - 16, songH);
        if (songShown) menus.setBounds(8, 174 + songH + 8, w - 16, juce::jmax(100, bottom - 174 - songH - 8));
        else if (heroShown < 0) menus.setBounds(8, 174, w - 16, bottom - 174);
        else menus.setBounds(8, 444, w - 16, bottom - 444);
        auto mb = machine.getBounds();           // (the FM-1's screen kept inside)
        if (! getLocalBounds().contains(mb)) machine.setTopLeftPosition(juce::jlimit(0, juce::jmax(0, w - mb.getWidth()), mb.getX()), juce::jlimit(0, juce::jmax(0, h - mb.getHeight()), mb.getY()));
        machine.setVisible(ed.showMachine && ! ed.machineSep);
        machine.toFront(false);
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
    machineSep = ui::settings().getUserSettings()->getBoolValue("machineSep", false);
    padsOpen = ui::settings().getUserSettings()->getBoolValue("padsOpen", false);
    root = std::make_unique<Root>(*this);
    addAndMakeVisible(*root);
    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(960, 600, 7680, 4320);
    setSize(1200, 750);
    startTimerHz(30);
    if (juce::SystemStats::getEnvironmentVariable("LOAF_TEST_OPEN", {}) == "gestion")   // (tests: the manager at once)
        juce::Timer::callAfterDelay(1500, [this] { openManager(); });
    if (juce::SystemStats::getEnvironmentVariable("LOAF_TEST_OPEN", {}) == "playlist")
        juce::Timer::callAfterDelay(1500, [this] { playlistEditor(); });
}

void LoafBreakerEditor::openManager()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(new ExchangeView(proc));
    o.dialogTitle = "LOAF BREAKER - GESTION (VST, BIBLIOTHEQUE, FM-1)";
    o.dialogBackgroundColour = ui::cur().bg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

LoafBreakerEditor::~LoafBreakerEditor() { stopTimer(); machineWin.reset(); }

void LoafBreakerEditor::resized()
{
    // the drawing scaled to the window (FLEXOR: "adapté à tous les écrans"): 1440 x 900 or more, its shape the window's
    // (wider: more room across; taller: more for the menus), never bands around it
    const float sc = juce::jmin((float) getWidth() / W, (float) getHeight() / H);
    const int lw = juce::jmax(W, (int) std::floor((float) getWidth() / sc)), lh = juce::jmax(H, (int) std::floor((float) getHeight() / sc));
    root->setTransform(juce::AffineTransform::scale(sc));
    root->setBounds(0, 0, lw, lh);
    root->resized();
}

void LoafBreakerEditor::paint(juce::Graphics& g) { g.fillAll(ui::cur().bg); }

void LoafBreakerEditor::timerCallback()
{
    proc.idlePump();                     // (no audio running: the firmware runs on all the same)
    {                                    // the FM-1 as the controller: its state said, its hands on the pads
        const int cs = proc.controllerState();
        if (cs != ctlShown) {
            if (cs == 2) toast(juce::CharPointer_UTF8("FM-1 CONTR\xc3\x94LEUR : CONNECT\xc3\x89"));
            else if (cs == 1 && ctlShown == 2) toast(juce::CharPointer_UTF8("FM-1 CONTR\xc3\x94LEUR : PERDU (D\xc3\x89BRANCH\xc3\x89 ?)"));
            else if (cs == 1) toast(juce::CharPointer_UTF8("FM-1 CONTR\xc3\x94LEUR : EN ATTENTE DU FM-1"));
            ctlShown = cs;
            root->header.repaint();
        }
        const uint64_t h = (uint64_t) proc.fm1Keys() | (uint64_t) proc.fm1Buttons() << 32;
        if (h != ctlHeld) { ctlHeld = h; root->pads.repaint(); }
    }
    if (proc.modelIfNew(modelSerial, m)) {
        root->header.repaint();
        root->cats.repaint();
        root->tabs.repaint();
        root->pads.repaint();
        root->wave.repaint();
        root->hero.repaint();
        if (ui::fm1SkinIs(m.skin)) root->repaint();   // (the VST follows the FM-1's skin: it changed)
        if (juce::String(m.dom) != lastDom) { lastDom = m.dom; opts.clear(); asked.clear(); }   // (another domain: its lists looked at again)
        if (seqView() != root->seqShown || songView() != root->songShown || heroKind() != root->heroShown) root->resized();
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
    {                                    // the sound's picture: wanted while it is shown, its view's columns and time
        const int hk = heroKind();
        if (hk >= 0 && hk <= 3) {
            proc.picWant(hk, root->hero.ms(), juce::jmax(200, (int) root->hero.WW));
            if (proc.picIfNew(picSerial, pic)) root->hero.repaint();
        } else
            proc.picWant(-1, 0, 0);
    }
    // the answers awaited: each setting's values
    for (auto it = asked.begin(); it != asked.end();) {
        lb_opts_t* o = optBuf.get();
        if (! proc.takeOptions(it->first, *o)) { ++it; continue; }
        if (o->n > 0) {
            Opts v;
            for (int i = 0; i < o->n; ++i) v.v.add(juce::String(o->v[i]));
            v.wrap = o->wrap != 0;
            v.more = o->more != 0;
            if (v.wrap && v.v.size() > 2) {   // (a list that goes round starts where the FM-1 is: shown from its natural
                                              // first, 909, or number 1)
                int at = -1;
                for (int i = 0; i < v.v.size() && at < 0; ++i)
                    if (v.v[i].trim() == "909" || v.v[i].trim().endsWith(" 1") || v.v[i].trim() == "1") at = i;
                if (at > 0) {
                    juce::StringArray r;
                    for (int i = 0; i < v.v.size(); ++i) r.add(v.v[(at + i) % v.v.size()]);
                    v.v = r;
                }
            }
            opts[it->second] = v;
        }
        it = asked.erase(it);
        root->menus.content.update();
    }
    if (songView() && proc.tracksIfNew(tracksSerial, root->song.tr) && root->song.dragI < 0) root->song.repaint();
    if (seqView()) {
        if (proc.seqIfNew(seqSerial, seq)) root->seqp.repaint();
        if (proc.patternsIfNew(patsSerial, pats)) root->seqp.repaint();
    } else if (songView() && proc.patternsIfNew(patsSerial, pats))
        root->song.repaint();                    // (the blocks' pictures)
    float l, r;
    proc.meters(l, r);
    root->header.meters(l, r);
    if (showMachine) {
        root->machine.update();
        if (machineSep && ! machineWin) machineShow();
        if (machineWin) machineWin->view.repaint();
    }
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
int LoafBreakerEditor::heroKind() const
{
    const juce::String d(m.dom), l(m.label);
    if (seqView()) return -1;
    if (songView()) return -1;
    if (d == "SOUNDS" && l == "KICK") return 0;
    if (d == "SOUNDS" && l == "DRUMS") return juce::jlimit(1, 3, m.tab_on + 1);
    return 9;
}
void LoafBreakerEditor::togglePads()
{
    padsOpen = ! padsOpen;
    ui::settings().getUserSettings()->setValue("padsOpen", padsOpen);
    root->resized();
    root->repaint();
}
bool LoafBreakerEditor::songView() const
{
    if (juce::String(m.dom) != "SONG") return false;
    return m.tab_on >= 0 && m.tab_on < m.ntabs && juce::String(m.tabs[m.tab_on]) == "TRACKS";
}
bool LoafBreakerEditor::seqView() const
{
    if (juce::String(m.dom) != "PATTERN") return false;
    const juce::String t = m.tab_on >= 0 && m.tab_on < m.ntabs ? juce::String(m.tabs[m.tab_on]) : juce::String();
    return t != "GROOVE" && t != "SETTINGS";
}
// PERSO: each of the skin's nine colours picked (the one in use to start from)
struct PersoEditor : juce::Component, juce::ChangeListener
{
    explicit PersoEditor(std::function<void()> changed) : onChange(std::move(changed))
    {
        k = ui::cur();
        sel.setCurrentColour(*field(0), juce::dontSendNotification);
        sel.addChangeListener(this);
        addAndMakeVisible(sel);
        setSize(560, 380);
    }
    std::function<void()> onChange;
    ui::Skin k;
    int pick = 0;
    juce::ColourSelector sel { juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders | juce::ColourSelector::showColourAtTop };
    static constexpr int N = 9;
    juce::Colour* field(int i) { juce::Colour* f[N] = { &k.bg, &k.panel, &k.panel2, &k.line, &k.ink, &k.dim, &k.accent, &k.hot, &k.white }; return f[i]; }
    static const char* label(int i) { static const char* L[N] = { "FOND", "PANNEAUX", "CASES (PAS VIDES)", "LIGNES", "TEXTE", "LABELS", "ACCENT (PAS ALLUMES)", "VIF (LIGNE CHOISIE)", "BLANC (TETE DE LECTURE)" }; return L[i]; }
    juce::Rectangle<int> row(int i) const { return { 10, 10 + i * 40, 220, 34 }; }
    void resized() override { sel.setBounds(240, 10, 310, 360); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(k.bg);
        for (int i = 0; i < N; ++i) {
            auto r = row(i);
            g.setColour(*field(i));
            g.fillRect(r.removeFromLeft(34));
            g.setColour(i == pick ? k.hot : k.line);
            g.drawRect(row(i), i == pick ? 2 : 1);
            g.setColour(k.ink);
            g.setFont(ui::label(13.0f));
            g.drawText(label(i), r.reduced(8, 0), juce::Justification::centredLeft);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < N; ++i)
            if (row(i).contains(e.getPosition())) { pick = i; sel.setCurrentColour(*field(i), juce::dontSendNotification); repaint(); }
    }
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        *field(pick) = sel.getCurrentColour();
        ui::setPerso(k);
        ui::setSkin(ui::persoIndex());
        repaint();
        if (onChange) onChange();
    }
};

void LoafBreakerEditor::persoEditor()
{
    juce::Component::SafePointer<LoafBreakerEditor> self(this);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(new PersoEditor([self] { if (self) self->root->repaint(); }));
    o.dialogTitle = "LOAF BREAKER - SKIN PERSO";
    o.dialogBackgroundColour = ui::cur().bg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

// the breaks by category, as the site's BREAKS page has them (breaks.index): a click plays it (at the next bar); the
// playlist's own submenu; this break added to it; the playlist edited
void LoafBreakerEditor::breakMenu()
{
    const auto all = proc.breaks();
    const int now = proc.breakNow();
    static const char* ORDER[5] = { "FLEXOR", "CYBERWORM AMENS", "HUGE AMEN PACK", "CLASSICS", "MES BREAKS" };
    juce::PopupMenu m;
    auto pl = proc.playlist();
    juce::PopupMenu plm;
    for (int k = 0; k < pl.size(); ++k)
        for (int i = 0; i < (int) all.size(); ++i)
            if (all[(size_t) i].name == pl[k]) { plm.addItem(1 + i, juce::String(k + 1) + ".  " + pl[k], true, i == now); break; }
    m.addSectionHeader("BREAKS  (" + juce::String((int) all.size()) + ")");
    for (auto* cat : ORDER) {
        juce::PopupMenu sub;
        int n = 0;
        for (int i = 0; i < (int) all.size(); ++i)
            if (all[(size_t) i].cat == cat) {
                const auto& b = all[(size_t) i];
                sub.addItem(1 + i, b.name + (b.title.isNotEmpty() ? "   -   " + b.title : juce::String()) + (pl.contains(b.name) ? "   [P]" : ""), true, i == now);
                n++;
            }
        if (n) m.addSubMenu(juce::String(cat) + "  (" + juce::String(n) + ")", sub, true, juce::Image(), now >= 0 && now < (int) all.size() && all[(size_t) now].cat == cat);
    }
    m.addSeparator();
    m.addSubMenu("PLAYLIST (" + juce::String(pl.size()) + ")", plm, pl.size() > 0);
    if (now >= 0 && now < (int) all.size()) {
        const auto name = all[(size_t) now].name;
        if (! pl.contains(name)) m.addItem(9001, "Ajouter " + name + " a la playlist");
        else m.addItem(9002, "Retirer " + name + " de la playlist");
    }
    m.addItem(9003, "Gerer la playlist (ordre, ajouts)...");
    juce::Component::SafePointer<LoafBreakerEditor> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&root->header).withMaximumNumColumns(1), [self, all, now](int r) {
        if (! self || r == 0) return;
        auto pl2 = self->proc.playlist();
        if (r == 9001 && now >= 0) { pl2.add(all[(size_t) now].name); self->proc.setPlaylist(pl2); }
        else if (r == 9002 && now >= 0) { pl2.removeString(all[(size_t) now].name); self->proc.setPlaylist(pl2); }
        else if (r == 9003) self->playlistEditor();
        else if (r >= 1 && r <= (int) all.size()) self->proc.breakGo(r - 1);
        self->root->header.repaint();
    });
}

// the playlist: its breaks in their order (drag a row to move it), x takes one out, + adds from the categories
struct PlaylistEditor : juce::Component
{
    explicit PlaylistEditor(LoafBreakerProcessor& p) : proc(p) { list = proc.playlist(); setSize(460, 560); }
    LoafBreakerProcessor& proc;
    juce::StringArray list;
    int drag = -1, hover = -1, top = 0;
    static constexpr int ROWS = 18;
    juce::Rectangle<int> row(int i) const { return { 12, 56 + i * 26, getWidth() - 24, 24 }; }
    juce::Rectangle<int> addBtn() const { return { 12, 12, 200, 32 }; }
    juce::Rectangle<int> clearBtn() const { return { 220, 12, 110, 32 }; }
    void save() { proc.setPlaylist(list); repaint(); }
    void paint(juce::Graphics& g) override
    {
        const auto& sk = ui::cur();
        g.fillAll(sk.bg);
        ui::button(g, addBtn().toFloat(), "+ AJOUTER DES BREAKS", false, false, 12.0f);
        ui::button(g, clearBtn().toFloat(), "TOUT VIDER", false, false, 12.0f);
        g.setColour(sk.dim);
        g.setFont(ui::label(11.0f));
        g.drawText("Glisser une ligne: la deplacer.  x: la retirer.  < > en haut: le break d'avant, d'apres.", 12, getHeight() - 26, getWidth() - 24, 18, juce::Justification::centredLeft);
        for (int i = 0; i < ROWS && top + i < list.size(); ++i) {
            const int k = top + i;
            auto r = row(i);
            g.setColour(k == drag ? sk.accent : k == hover ? sk.panel2.brighter(0.08f) : sk.panel2);
            g.fillRect(r);
            g.setColour(k == drag ? sk.bg : sk.ink);
            g.setFont(ui::value(15.0f));
            g.drawText(juce::String(k + 1) + ".", r.withWidth(36).translated(6, 0), juce::Justification::centredLeft);
            g.setFont(ui::label(13.0f));
            g.drawText(list[k], r.withTrimmedLeft(44), juce::Justification::centredLeft);
            g.setColour(sk.dim);
            g.drawText("x", r.withLeft(r.getRight() - 24), juce::Justification::centred);
        }
        if (list.isEmpty()) {
            g.setColour(sk.dim);
            g.setFont(ui::label(14.0f));
            g.drawText("Vide: < > passent par tous les breaks.", getLocalBounds(), juce::Justification::centred);
        }
    }
    int at(juce::Point<int> p) const { for (int i = 0; i < ROWS; ++i) if (row(i).contains(p) && top + i < list.size()) return top + i; return -1; }
    void mouseMove(const juce::MouseEvent& e) override { hover = at(e.getPosition()); repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (addBtn().contains(e.getPosition())) { addMenu(); return; }
        if (clearBtn().contains(e.getPosition())) { list.clear(); save(); return; }
        const int k = at(e.getPosition());
        if (k < 0) return;
        if (e.x > row(0).getRight() - 24) { list.remove(k); save(); return; }
        drag = k;
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (drag < 0) return;
        const int k = juce::jlimit(0, list.size() - 1, top + (e.y - 56) / 26);
        if (k != drag) { list.move(drag, k); drag = k; repaint(); }
    }
    void mouseUp(const juce::MouseEvent&) override { if (drag >= 0) { drag = -1; save(); } }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        top = juce::jlimit(0, juce::jmax(0, list.size() - ROWS), top + (w.deltaY < 0 ? 2 : -2));
        repaint();
    }
    void addMenu()
    {
        const auto all = proc.breaks();
        static const char* ORDER[5] = { "FLEXOR", "CYBERWORM AMENS", "HUGE AMEN PACK", "CLASSICS", "MES BREAKS" };
        juce::PopupMenu m;
        for (int c = 0; c < 5; ++c) {
            juce::PopupMenu sub;
            int n = 0;
            sub.addItem(10000 + c, "TOUS (" + juce::String(ORDER[c]) + ")");
            sub.addSeparator();
            for (int i = 0; i < (int) all.size(); ++i)
                if (all[(size_t) i].cat == ORDER[c]) { sub.addItem(1 + i, all[(size_t) i].name, ! list.contains(all[(size_t) i].name)); n++; }
            if (n) m.addSubMenu(ORDER[c], sub);
        }
        juce::Component::SafePointer<PlaylistEditor> self(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [self, all](int r) {
            if (! self || r == 0) return;
            if (r >= 10000) {
                for (auto& b : all) if (b.cat == ORDER[r - 10000] && ! self->list.contains(b.name)) self->list.add(b.name);
            } else if (r <= (int) all.size())
                self->list.addIfNotAlreadyThere(all[(size_t) r - 1].name);
            self->save();
        });
    }
};

void LoafBreakerEditor::playlistEditor()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(new PlaylistEditor(proc));
    o.dialogTitle = "LOAF BREAKER - PLAYLIST DE BREAKS";
    o.dialogBackgroundColour = ui::cur().bg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

// full screen (FLEXOR: "un mode fullscreen et borderless fullscreen"). The Standalone: 1 the true full screen (the
// whole monitor, the taskbar under it: kiosk mode), 2 borderless (a window without its bar, the monitor's size); ESC or
// the same key again: back to the window as it was. In a DAW the plugin's window is the host's: the plugin grows to
// the screen instead (its bar stays the host's)
void LoafBreakerEditor::screenMode(int mode)
{
    auto* top = getTopLevelComponent();
    auto* dw = findParentComponentOfClass<juce::DocumentWindow>();
    const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone && dw != nullptr;
    if (mode == screen) mode = 0;            // (the same again: back)
    auto& desk = juce::Desktop::getInstance();
    const auto* disp = desk.getDisplays().getDisplayForRect(top->getScreenBounds());
    if (! standalone) {                      // (a DAW's window: as big as the screen allows, or back)
        if (mode && disp) {
            windowBounds = getBounds();
            const auto area = disp->userArea.reduced(10, 40);
            setSize(area.getWidth(), area.getHeight());   // (the screen's shape: the drawing follows it)
        } else if (! windowBounds.isEmpty())
            setSize(windowBounds.getWidth(), windowBounds.getHeight());
        screen = mode;
        toast(mode ? "AGRANDI (ECHAP: REVENIR)" : "FENETRE");
        return;
    }
    if (screen == 0) {                       // (leaving the window: its place kept)
        windowBounds = dw->getBounds();
        titleH = dw->getTitleBarHeight();
        nativeBar = dw->isUsingNativeTitleBar();
    }
    if (desk.getKioskModeComponent() != nullptr) desk.setKioskModeComponent(nullptr, false);
    if (screen == 3 && mode != 3) dw->setFullScreen(false);
    if (mode == 3) {                         // (FLEXOR: "plein écran avec bordure": the window maximised, its bar kept)
        dw->setUsingNativeTitleBar(nativeBar);
        dw->setTitleBarHeight(titleH > 0 ? titleH : 26);
        dw->setFullScreen(true);
    } else if (mode == 1) {
        dw->setUsingNativeTitleBar(false);
        dw->setTitleBarHeight(0);
        desk.setKioskModeComponent(dw, false);
    } else if (mode == 2 && disp) {
        dw->setUsingNativeTitleBar(false);
        dw->setTitleBarHeight(0);
        dw->setBounds(disp->totalArea);
    } else {
        dw->setUsingNativeTitleBar(nativeBar);
        dw->setTitleBarHeight(titleH > 0 ? titleH : 26);
        if (! windowBounds.isEmpty()) dw->setBounds(windowBounds);
    }
    screen = mode;
    toast(mode == 1 ? "PLEIN ECRAN (ECHAP: REVENIR)" : mode == 2 ? "PLEIN ECRAN SANS BORDURE (ECHAP: REVENIR)" : mode == 3 ? "AGRANDI, AVEC SA BORDURE" : "FENETRE");
    grabKeyboardFocus();
}

void LoafBreakerEditor::toggleMachine()
{
    showMachine = ! showMachine;
    ui::settings().getUserSettings()->setValue("machine", showMachine);
    machineShow();
}

// the FM-1's screen in the editor (a floating box) or in a window of its own
void LoafBreakerEditor::machineSeparate(bool sep)
{
    machineSep = sep;
    showMachine = true;
    ui::settings().getUserSettings()->setValue("machine", true);
    ui::settings().getUserSettings()->setValue("machineSep", sep);
    machineShow();
}

void LoafBreakerEditor::machineShow()
{
    if (showMachine && machineSep) {
        if (! machineWin) machineWin = std::make_unique<MachineWindow>(*this, root->machine.screen);
    } else
        machineWin.reset();
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
    juce::PopupMenu skins, fm1Skins;
    for (int i = 0; i < ui::fm1First(); ++i) skins.addItem(100 + i, ui::skin(i).name, true, i == ui::curIndex());
    skins.addSeparator();
    skins.addItem(99, "COMME LE FM-1 (son SETUP)", true, ui::curIndex() == ui::FOLLOW_FM1);
    for (int i = ui::fm1First(); i < ui::persoIndex(); ++i) fm1Skins.addItem(100 + i, ui::skin(i).name, true, i == ui::curIndex());
    skins.addSubMenu("Les 24 du FM-1", fm1Skins);
    skins.addSeparator();
    skins.addItem(100 + ui::persoIndex(), "PERSO", true, ui::persoIndex() == ui::curIndex());
    skins.addItem(98, "Regler PERSO (couleur par couleur)...");
    juce::PopupMenu fm1;
    fm1.addItem(206, juce::CharPointer_UTF8("Jouer le VST avec le FM-1 (contr\xc3\xb4leur, USB)"), true, proc.controller());
    fm1.addSeparator();
    fm1.addItem(205, "GESTION: kicks, patterns, tracks, bibliotheque...");
    fm1.addSeparator();
    fm1.addItem(201, "Envoyer tout au FM-1");
    fm1.addItem(202, "Recuperer tout du FM-1");
    fm1.addItem(203, "Exporter une sauvegarde FM-1 (.bak)...");
    fm1.addItem(204, "Importer une sauvegarde FM-1 (.bak)...");
    juce::PopupMenu mm;
    mm.addItem(3, "GESTION: VST, bibliotheque de kicks, FM-1...");
    mm.addSeparator();
    mm.addSubMenu("Skin", skins);
    mm.addItem(1, "Clavier (touches)...");
    {
        juce::PopupMenu ap;
        ap.addItem(2, "Afficher", true, showMachine);
        ap.addSeparator();
        ap.addItem(53, "Dans la fenetre (cadre flottant)", true, showMachine && ! machineSep);
        ap.addItem(54, "Dans une fenetre separee", true, showMachine && machineSep);
        mm.addSubMenu("Apercu du FM-1", ap);
    }
    {
        const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
        juce::PopupMenu sm;
        if (standalone) {
            sm.addItem(53 + 100, "Agrandir, avec bordure  (le bouton en haut)", true, screen == 3);
            sm.addItem(51, "Plein ecran  (F11)", true, screen == 1);
            sm.addItem(52, "Plein ecran sans bordure  (SHIFT+F11)", true, screen == 2);
        } else
            sm.addItem(51, "Agrandir a tout l'ecran  (F11)", true, screen != 0);
        sm.addItem(50, "Fenetre  (ECHAP)", screen != 0, screen == 0);
        mm.addSubMenu("Affichage", sm);
    }
    mm.addSeparator();
    mm.addSubMenu("Machine FM-1", fm1);
    juce::Component::SafePointer<LoafBreakerEditor> self(this);
    mm.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&root->header), [self](int r) {
        if (! self || r == 0) return;
        if (r >= 100 && r < 200) { ui::setSkin(r - 100); self->root->repaint(); return; }
        if (r == 99) { ui::setSkin(ui::FOLLOW_FM1); self->root->repaint(); return; }
        if (r == 98) { self->persoEditor(); return; }
        if (r == 1) { self->keysEditor(); return; }
        if (r == 2) { self->toggleMachine(); return; }
        if (r == 3) { self->openManager(); return; }
        if (r >= 50 && r <= 52) { self->screenMode(r - 50); return; }
        if (r == 153) { self->screenMode(3); return; }
        if (r == 53 || r == 54) { self->machineSeparate(r == 54); return; }
        if (r == 205) { self->openManager(); return; }
        if (r == 206) {
            const bool on = ! self->proc.controller();
            self->proc.setController(on);
            if (! on) { self->ctlShown = 0; self->toast(juce::CharPointer_UTF8("FM-1 CONTR\xc3\x94LEUR : ARR\xc3\x8aT\xc3\x89")); }
            self->root->repaint();
            return;
        }
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
    if (k.getKeyCode() == juce::KeyPress::escapeKey && screen != 0) { screenMode(0); return true; }
    const int ctx = seqView() ? 1 : 2;   // (the grid's keys there, the menus' elsewhere; then everywhere's)
    const int a = KeyMap::get().actionOf(k, ctx);
    if (a < 0) return false;
    const auto& act = KeyMap::get().actions()[(size_t) a];
    switch (act.kind) {
    case KeyAction::Turn:
        if (act.index == KeyAction::Tempo) proc.send(LoafBreakerProcessor::CmdTempo, 0, act.dir);
        else if (act.index == KeyAction::BreakStep) proc.breakStep(act.dir);
        else turn(act.index, act.dir);
        break;
    case KeyAction::Button:
        if (act.index != LB_SEL && act.index != LB_REC) tap(act.index);
        break;
    case KeyAction::Seq: root->seqp.command(act.index); break;
    case KeyAction::Menu: root->menus.content.command(act.index); break;
    case KeyAction::Nav: nav(act.index); break;
    default: break;                      // (slices and pads: held, keyStateChanged)
    }
    return true;
}

void LoafBreakerEditor::nav(int i)
{
    if (i >= KeyAction::NavDomain0 && i < KeyAction::NavDomain0 + NDOMS) { goDomain(i - KeyAction::NavDomain0); return; }
    switch (i) {
    case KeyAction::NavTabNext: turn(LB_SELECT, 1); break;
    case KeyAction::NavTabPrev: turn(LB_SELECT, -1); break;
    case KeyAction::NavSave: tap(LB_SAVE); toast("SAUVE"); break;
    case KeyAction::NavUndo: proc.step(6, 0, 0, 0, 0); toast("ANNULE"); break;
    case KeyAction::NavCopy: proc.step(7, 0, 0, 0, 0); toast("PATTERN COPIE"); break;
    case KeyAction::NavPaste: proc.step(8, 0, 0, 0, 0); toast("PATTERN COLLE"); break;
    case KeyAction::NavBreaks: breakMenu(); break;
    case KeyAction::NavPlaylist: playlistEditor(); break;
    case KeyAction::NavManager: openManager(); break;
    case KeyAction::NavScreen: toggleMachine(); break;
    case KeyAction::NavKeys: keysEditor(); break;
    case KeyAction::NavFull: screenMode(1); break;
    case KeyAction::NavBorderless: screenMode(proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone ? 2 : 1); break;
    default: break;
    }
}

void LoafBreakerEditor::keysEditor()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(new KeyMapEditor());
    o.dialogTitle = "LOAF BREAKER - CLAVIER";
    o.dialogBackgroundColour = ui::cur().bg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

bool LoafBreakerEditor::keyStateChanged(bool)
{
    bool used = false;
    auto& km = KeyMap::get();
    for (size_t a = 0; a < km.actions().size(); ++a) {
        const auto& act = km.actions()[a];
        const bool holdable = act.kind == KeyAction::White || act.kind == KeyAction::Black ||
                              (act.kind == KeyAction::Button && (act.index == LB_SEL || act.index == LB_REC));
        const auto& kp = km.keyOf((int) a);
        if (! holdable || ! kp.isValid()) continue;
        const bool down = juce::KeyPress::isKeyCurrentlyDown(kp.getKeyCode()) &&
                          ! juce::ModifierKeys::getCurrentModifiers().isCommandDown() && ! juce::ModifierKeys::getCurrentModifiers().isCtrlDown();
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

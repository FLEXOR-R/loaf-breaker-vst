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
    float vl = 0, vr = 0, hl = 0, hr = 0;
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
        g.drawText("TEMPO", 510, 14, 120, 12, juce::Justification::centredLeft);
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
            if (r.getRight() > 1040.0f) break;
            ui::button(g, r, t, i == ed.m.tab_on, hover == i, 11.0f);
            cells.push_back({ r, i });
            x += w + 4.0f;
        }
        // the part ALGORITHM turns: < PART >
        if (ed.m.part[0]) {
            juce::Rectangle<float> l(1060, 4, 34, 26), p(1098, 4, 290, 26), rr(1392, 4, 34, 26);
            ui::button(g, l, "<", false, hover == -10, 13.0f);
            ui::button(g, rr, ">", false, hover == -11, 13.0f);
            g.setColour(sk.panel2);
            g.fillRect(p);
            g.setColour(sk.line);
            g.drawRect(p, 1.0f);
            g.setColour(sk.accent);
            ui::fitText(g, juce::String(ed.m.part), p.reduced(8, 0), ui::label(12.0f));
            cells.push_back({ l, -10 });
            cells.push_back({ rr, -11 });
        }
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
        } else if (h == -10 || h == -11)
            ed.turn(LB_ALGO, h == -10 ? -1 : 1);
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

// ------------------------------------------------------------------------------------------------ the knobs ---
struct Knob : Part
{
    Knob(LBE& e, int k) : Part(e), idx(k) {}
    int idx;
    float drag = 0.0f, spin = 0.0f;
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        auto r = getLocalBounds().toFloat();
        const juce::String name = ed.m.kname[idx], val = ed.m.kval[idx];
        const bool none = name.isEmpty();
        g.setColour(none ? sk.dim.withAlpha(0.4f) : sk.ink);
        ui::fitText(g, none ? juce::String("-") : name, r.removeFromTop(22.0f), ui::labelBold(13.0f));
        auto dial = r.removeFromTop(r.getHeight() - 34.0f);
        const float d = juce::jmin(dial.getWidth(), dial.getHeight()) - 6.0f;
        auto c = dial.getCentre();
        const float norm = ed.m.knorm[idx];
        const int segs = 28;
        const float a0 = juce::MathConstants<float>::pi * 0.75f, span = juce::MathConstants<float>::pi * 1.5f;
        for (int i = 0; i < segs; ++i) {
            const float t = (float) i / (segs - 1);
            const float ang = a0 + span * t;
            const float rr = d * 0.5f;
            juce::Rectangle<float> sq(c.x + std::cos(ang) * rr - 3.0f, c.y + std::sin(ang) * rr - 3.0f, 6.0f, 6.0f);
            bool lit;
            if (norm >= 0.0f) lit = t <= norm + 1e-3f;
            else lit = std::abs(t - std::fmod(spin + 10.0f, 1.0f)) < 0.5f / segs;   // (no gauge: a turning mark)
            g.setColour(none ? sk.line.withAlpha(0.4f) : lit ? sk.accent : sk.line);
            g.fillRect(sq.toNearestInt());
        }
        // the body: a dark disc with a grain, a notch
        g.setColour(sk.panel2);
        g.fillEllipse(juce::Rectangle<float>(d * 0.62f, d * 0.62f).withCentre(c));
        g.setColour(sk.line);
        g.drawEllipse(juce::Rectangle<float>(d * 0.62f, d * 0.62f).withCentre(c), 1.0f);
        const float na = norm >= 0.0f ? a0 + span * norm : a0 + span * std::fmod(spin + 10.0f, 1.0f);
        g.setColour(none ? sk.dim : sk.hot);
        g.fillRect(juce::Rectangle<float>(4.0f, 4.0f).withCentre({ c.x + std::cos(na) * d * 0.2f, c.y + std::sin(na) * d * 0.2f }));
        g.setColour(none ? sk.dim : sk.hot);
        ui::fitText(g, none ? juce::String() : val, r, ui::value(30.0f));
    }
    void mouseDown(const juce::MouseEvent&) override { drag = 0.0f; }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const float y = (float) -e.getDistanceFromDragStartY();
        const int n = (int) ((y - drag) / 7.0f);
        if (n != 0) { drag += n * 7.0f; send(n); }
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int n = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
        if (n) send(n);
    }
    void mouseDoubleClick(const juce::MouseEvent&) override {}
    void send(int n) { ed.turn(LB_K1 + idx, n); spin += n * 0.04f; repaint(); }
};

struct Controls : Part
{
    explicit Controls(LBE& e) : Part(e)
    {
        for (int i = 0; i < 4; ++i) { knobs.add(new Knob(e, i)); addAndMakeVisible(knobs.getLast()); }
        auto mk = [&](const char* t, std::function<void(bool)> f) { auto* b = new PixButton(e, t, std::move(f)); btns.add(b); addAndMakeVisible(b); return b; };
        shift = mk("SHIFT", [&e](bool d) { e.hold(LB_SEL, d); });
        mk("PRESET <", [&e](bool d) { if (d) e.turn(LB_PRESET, -1); });
        mk("PRESET >", [&e](bool d) { if (d) e.turn(LB_PRESET, 1); });
        mk("SAVE", [&e](bool d) { if (d) e.tap(LB_SAVE); });
        mk("OCT -", [&e](bool d) { e.hold(LB_OCTDN, d); });
        mk("OCT +", [&e](bool d) { e.hold(LB_OCTUP, d); });
    }
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<PixButton> btns;
    PixButton* shift = nullptr;
    void resized() override
    {
        auto r = getLocalBounds().reduced(16, 20);
        auto side = r.removeFromRight(130);
        auto kr = r;
        const int w = kr.getWidth() / 4;
        for (int i = 0; i < 4; ++i) knobs[i]->setBounds(kr.getX() + i * w + 6, kr.getY(), w - 12, kr.getHeight());
        side.removeFromTop(6);
        for (auto* b : btns) { b->setBounds(side.removeFromTop(30)); side.removeFromTop(6); }
    }
    void paint(juce::Graphics& g) override
    {
        ui::panel(g, getLocalBounds().toFloat().reduced(1.0f), ed.m.sel ? "SHIFT: LES KNOBS DE SEL" : "CONTROLES");
        shift->on = ed.m.sel != 0;
    }
};

// ----------------------------------------------------------------------------------- the FM-1's screen, scope ---
struct Machine : Part
{
    explicit Machine(LBE& e) : Part(e) {}
    juce::Image screen { juce::Image::RGB, 240, 240, true };
    uint32_t serial = 0;
    std::vector<uint16_t> px = std::vector<uint16_t>(240 * 240);
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
    void paint(juce::Graphics& g) override
    {
        ui::panel(g, getLocalBounds().toFloat().reduced(1.0f), "ECRAN FM-1");
        auto r = getLocalBounds().toFloat().reduced(14.0f, 18.0f);
        const float sz = juce::jmin(r.getWidth(), r.getHeight());
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImage(screen, r.withSizeKeepingCentre(sz, sz));
    }
};

struct Scope : Part
{
    explicit Scope(LBE& e) : Part(e) {}
    float buf[512] {};
    void paint(juce::Graphics& g) override
    {
        const auto& sk = s();
        ui::panel(g, getLocalBounds().toFloat().reduced(1.0f), "SORTIE");
        auto r = getLocalBounds().toFloat().reduced(14.0f, 18.0f);
        g.setColour(sk.line);
        for (float y = r.getY(); y < r.getBottom(); y += 12.0f) g.fillRect(r.getX(), y, r.getWidth(), 1.0f);
        ed.proc.scope(buf, 512);
        g.setColour(sk.accent);
        for (int i = 0; i < 512; i += 2) {
            const float x = r.getX() + r.getWidth() * i / 512.0f;
            const float y = r.getCentreY() - juce::jlimit(-1.0f, 1.0f, buf[i]) * r.getHeight() * 0.48f;
            g.fillRect(x, y, 2.0f, 2.0f);
        }
    }
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
    explicit Root(LBE& e) : Part(e), header(e), cats(e), tabs(e), wave(e), controls(e), machine(e), scope(e), pads(e)
    {
        for (juce::Component* c : std::initializer_list<juce::Component*> { &header, &cats, &tabs, &wave, &controls, &machine, &scope, &pads })
            addAndMakeVisible(c);
        setSize(W, H);
    }
    Header header;
    Categories cats;
    Tabs tabs;
    WavePanel wave;
    Controls controls;
    Machine machine;
    Scope scope;
    Pads pads;
    void resized() override
    {
        header.setBounds(0, 0, W, 86);
        cats.setBounds(0, 86, W, 44);
        tabs.setBounds(0, 130, W, 36);
        wave.setBounds(8, 172, W - 16, 236);
        const bool mach = ed.showMachine;
        controls.setBounds(8, 414, mach ? 820 : 1100, 268);
        machine.setVisible(mach);
        machine.setBounds(834, 414, 268, 268);
        scope.setBounds(mach ? 1108 : 1114, 414, mach ? W - 1116 : W - 1122, 268);
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
        root->controls.repaint();
        for (auto* k : root->controls.knobs) k->repaint();
        root->pads.repaint();
        root->wave.repaint();
        // a source asked for (SOUNDS: KICK, DRUMS, BREAK): SEL + EDIT until it is the one
        if (pendingSource.isNotEmpty()) {
            if (juce::String(m.label) == pendingSource || ++sourceTries > 4) pendingSource.clear();
            else if (sourceTries % 2 == 0) { hold(LB_SEL, true); tap(LB_EDIT); juce::Timer::callAfterDelay(120, [this] { hold(LB_SEL, false); }); }
        }
    }
    if (proc.waveIfNew(waveSerial, wave))
        root->wave.repaint();
    float l, r;
    proc.meters(l, r);
    root->header.meters(l, r);
    root->scope.repaint();
    if (showMachine) root->machine.update();
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
    mm.addItem(2, "Ecran du FM-1", true, showMachine);
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
        if (r == 2) {
            self->showMachine = ! self->showMachine;
            ui::settings().getUserSettings()->setValue("machine", self->showMachine);
            self->root->resized();
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
    int code = k.getKeyCode();
    if (code >= 'A' && code <= 'Z') code += 'a' - 'A';
    const int a = KeyMap::get().actionOf(code);
    if (a < 0) return false;
    const auto& act = KeyMap::get().actions()[(size_t) a];
    if (act.kind == KeyAction::Turn) turn(act.index, act.dir);
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

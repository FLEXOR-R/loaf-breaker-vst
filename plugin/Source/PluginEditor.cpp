#include "PluginEditor.h"

namespace {
const juce::Colour BG(0xff141012), PANEL(0xff201a1c), INK(0xfff2ecec), DIM(0xff8f8587), RED(0xffff4a57);
const char* const BTN_NAME[14] = { "FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY",
                                   "REC", "OCT-", "OCT+" };
const char* const ENC_NAME[7] = { "SELECT", "ALGORITHM", "PRESETS", "K1", "K2", "K3", "K4" };
// the FM-1's keys, F3 .. G5: black where a piano's are
bool isBlack(int k) { const int n = (k + 5) % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }
// the computer's keyboard: the white keys (the slices) on two letter rows, the black ones (the pads) on the numbers
const char* const WHITE_KB = "qwertyuiasdfghjk";
const char* const BLACK_KB = "1234567890-";
char kbOf(int k)
{
    int w = 0, b = 0;
    for (int i = 0; i < k; ++i) (isBlack(i) ? b : w)++;
    return isBlack(k) ? BLACK_KB[b] : WHITE_KB[w];
}
}

struct LoafBreakerEditor::PanelButton : juce::Component
{
    PanelButton(LoafBreakerProcessor& p, int b) : proc(p), id(b) { setName(BTN_NAME[b]); }
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(down ? RED : PANEL.brighter(0.15f));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(down ? BG : INK);
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText(getName(), r, juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent&) override { down = true; proc.send(LoafBreakerProcessor::CmdButton, id, 1); repaint(); }
    void mouseUp(const juce::MouseEvent&) override { down = false; proc.send(LoafBreakerProcessor::CmdButton, id, 0); repaint(); }
    LoafBreakerProcessor& proc;
    int id;
    bool down = false;
};

struct LoafBreakerEditor::Encoder : juce::Component
{
    Encoder(LoafBreakerProcessor& p, int r) : proc(p), role(r) { setName(ENC_NAME[r]); }
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto knob = r.removeFromTop(r.getHeight() - 18.0f).reduced(6.0f);
        const float d = juce::jmin(knob.getWidth(), knob.getHeight());
        knob = knob.withSizeKeepingCentre(d, d);
        g.setColour(PANEL.brighter(0.2f));
        g.fillEllipse(knob);
        g.setColour(RED);
        const float a = angle;
        auto c = knob.getCentre();
        g.drawLine(c.x, c.y, c.x + std::sin(a) * d * 0.42f, c.y - std::cos(a) * d * 0.42f, 3.0f);
        g.setColour(DIM);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(getName(), r, juce::Justification::centred);
    }
    void turn(int n)
    {
        if (n == 0) return;
        proc.send(LoafBreakerProcessor::CmdTurn, role, n);
        angle += 0.26f * (float) n;
        repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override { lastY = e.position.y; }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const float dy = lastY - e.position.y;
        const int n = (int) (dy / 8.0f);
        if (n != 0) { turn(n); lastY -= (float) n * 8.0f; }
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        acc += w.deltaY * 8.0f;
        const int n = (int) acc;
        if (n != 0) { turn(n); acc -= (float) n; }
    }
    LoafBreakerProcessor& proc;
    int role;
    float lastY = 0.0f, acc = 0.0f, angle = 0.0f;
};

struct LoafBreakerEditor::Keys : juce::Component
{
    explicit Keys(LoafBreakerProcessor& p) : proc(p) {}
    juce::Rectangle<float> keyRect(int k) const
    {
        // white keys side by side; a black one over the gap before its white neighbour
        int whites = 0;
        for (int i = 0; i < 27; ++i) if (! isBlack(i)) whites++;
        const float w = (float) getWidth() / (float) whites, h = (float) getHeight();
        int wi = 0;
        for (int i = 0; i < k; ++i) if (! isBlack(i)) wi++;
        if (! isBlack(k))
            return { wi * w, 0.0f, w, h };
        return { wi * w - w * 0.32f, 0.0f, w * 0.64f, h * 0.6f };
    }
    int keyAt(juce::Point<float> p) const
    {
        for (int k = 0; k < 27; ++k) if (isBlack(k) && keyRect(k).contains(p)) return k;
        for (int k = 0; k < 27; ++k) if (! isBlack(k) && keyRect(k).contains(p)) return k;
        return -1;
    }
    void paint(juce::Graphics& g) override
    {
        for (int pass = 0; pass < 2; ++pass)
            for (int k = 0; k < 27; ++k) {
                if (isBlack(k) != (pass == 1)) continue;
                auto r = keyRect(k).reduced(1.5f);
                const bool on = held >> k & 1u;
                g.setColour(on ? RED : isBlack(k) ? juce::Colour(0xff2a2426) : juce::Colour(0xffe9e4e4));
                g.fillRoundedRectangle(r, 4.0f);
                g.setColour(on ? BG : isBlack(k) ? DIM : juce::Colour(0xff6c6264));
                g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
                g.drawText(juce::String::charToString((juce::juce_wchar) juce::CharacterFunctions::toUpperCase((juce::juce_wchar) kbOf(k))),
                           r.removeFromBottom(18.0f), juce::Justification::centred);
            }
    }
    void press(int k, bool down)
    {
        if (k < 0) return;
        const uint32_t bit = 1u << k;
        if (down == ((held & bit) != 0)) return;
        held = down ? held | bit : held & ~bit;
        proc.send(LoafBreakerProcessor::CmdKey, k, down ? 100 : 0);
        repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override { cur = keyAt(e.position); press(cur, true); }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const int k = keyAt(e.position);
        if (k != cur) { press(cur, false); cur = k; press(cur, true); }
    }
    void mouseUp(const juce::MouseEvent&) override { press(cur, false); cur = -1; }
    LoafBreakerProcessor& proc;
    uint32_t held = 0;
    int cur = -1;
};

LoafBreakerEditor::LoafBreakerEditor(LoafBreakerProcessor& p) : AudioProcessorEditor(&p), proc(p)
{
    for (int b = 0; b < 14; ++b)
        addAndMakeVisible(panel.add(new PanelButton(proc, b)));
    for (int e = 0; e < 7; ++e)
        addAndMakeVisible(panel.add(new Encoder(proc, e)));
    keys = std::make_unique<Keys>(proc);
    addAndMakeVisible(*keys);
    volume.setSliderStyle(juce::Slider::LinearHorizontal);
    volume.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volume.setColour(juce::Slider::thumbColourId, RED);
    addAndMakeVisible(volume);
    addAndMakeVisible(sync);
    status.setColour(juce::Label::textColourId, DIM);
    status.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(status);
    volumeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.params, "volume", volume);
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.params, "sync", sync);
    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(820, 560, 2400, 1640);
    getConstrainer()->setFixedAspectRatio(1040.0 / 700.0);
    setSize(1040, 700);
    startTimerHz(30);
}

LoafBreakerEditor::~LoafBreakerEditor() { stopTimer(); }

void LoafBreakerEditor::resized()
{
    auto r = getLocalBounds().reduced(16);
    auto top = r.removeFromTop(36);
    top.removeFromLeft(220);              // (the title, painted)
    status.setBounds(top.removeFromRight(160));
    sync.setBounds(top.removeFromRight(120));
    volume.setBounds(top.removeFromRight(200).reduced(0, 6));
    r.removeFromTop(10);
    keys->setBounds(r.removeFromBottom(juce::jmax(90, r.getHeight() / 6)));
    r.removeFromBottom(12);
    const int s = juce::jmin(r.getHeight(), r.getWidth() / 2);
    screenArea = r.removeFromLeft(s).withSizeKeepingCentre(s, s);
    r.removeFromLeft(20);
    // the knobs on top (SELECT ALGORITHM PRESETS, then K1-K4), the buttons below in two rows of seven
    auto knobs = r.removeFromTop(r.getHeight() / 2);
    auto row1 = knobs.removeFromTop(knobs.getHeight() / 2), row2 = knobs;
    for (int e = 0; e < 3; ++e)
        panel[14 + e]->setBounds(row1.removeFromLeft(row1.getWidth() / (3 - e)).reduced(4));
    for (int e = 3; e < 7; ++e)
        panel[14 + e]->setBounds(row2.removeFromLeft(row2.getWidth() / (7 - e)).reduced(4));
    r.removeFromTop(8);
    const int rows = 2, cols = 7, bh = r.getHeight() / rows;
    for (int b = 0; b < 14; ++b) {
        const int row = b / cols, col = b % cols;
        panel[b]->setBounds(r.getX() + col * r.getWidth() / cols, r.getY() + row * bh, r.getWidth() / cols, bh);
    }
}

void LoafBreakerEditor::paint(juce::Graphics& g)
{
    g.fillAll(BG);
    g.setColour(RED);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText("LOAF BREAKER", 16, 16, 260, 36, juce::Justification::centredLeft);
    g.setColour(juce::Colours::black);
    g.fillRect(screenArea.expanded(4));
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImage(screen, screenArea.toFloat());
}

void LoafBreakerEditor::timerCallback()
{
    proc.idlePump();                     // (no audio running: the firmware kept going all the same)
    if (proc.screenIfNew(serial, px.data())) {
        juce::Image::BitmapData bd(screen, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 240; ++y)
            for (int x = 0; x < 240; ++x) {
                const uint16_t v = px[(size_t) (y * 240 + x)];
                bd.setPixelColour(x, y, juce::Colour((uint8_t) ((v >> 11) << 3 | (v >> 13)),
                                                     (uint8_t) (((v >> 5) & 63) << 2 | ((v >> 9) & 3)),
                                                     (uint8_t) ((v & 31) << 3 | ((v >> 2) & 7))));
            }
        repaint(screenArea);
    }
    status.setText(proc.status(), juce::dontSendNotification);
}

bool LoafBreakerEditor::keyPressed(const juce::KeyPress& k)
{
    const auto c = (char) juce::CharacterFunctions::toLowerCase(k.getTextCharacter());
    if (k.getKeyCode() == juce::KeyPress::spaceKey) {          // space: PLAY
        proc.send(LoafBreakerProcessor::CmdButton, 10, 1);
        proc.send(LoafBreakerProcessor::CmdButton, 10, 0);
        return true;
    }
    return c != 0 && (std::strchr(WHITE_KB, c) != nullptr || std::strchr(BLACK_KB, c) != nullptr);
}

bool LoafBreakerEditor::keyStateChanged(bool)
{
    bool used = false;
    for (int k = 0; k < 27; ++k) {
        const bool down = juce::KeyPress::isKeyCurrentlyDown((int) (unsigned char) kbOf(k));
        if (down != ((heldKeys >> k & 1u) != 0)) {
            heldKeys ^= 1u << k;
            keys->press(k, down);
            used = true;
        }
    }
    return used;
}

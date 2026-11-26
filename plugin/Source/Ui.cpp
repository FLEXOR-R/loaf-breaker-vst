#include "Ui.h"
#include "BinaryData.h"
#include <map>

namespace ui {

// the VST's own (white: the playhead, a step's marks)
struct Raw { const char* name; uint32_t bg, panel, panel2, line, ink, dim, accent, hot, white; };
static const Raw OWN[] = {
    { "LOAF LORD", 0xff090909, 0xff111111, 0xff191919, 0xff2c2b29, 0xffe8e3d8, 0xff8a857b, 0xfff4efe4, 0xffffffff, 0xffffffff },
    { "BLOOD", 0xff0a0707, 0xff130d0d, 0xff1c1212, 0xff3a2222, 0xffeee2df, 0xff94817d, 0xffd51f2e, 0xffff4a57, 0xffffffff },
    { "TOXIC", 0xff070907, 0xff0e120d, 0xff151b13, 0xff26321f, 0xffe3eedb, 0xff7f9174, 0xff9fe32b, 0xffd4ff70, 0xffffffff },
    { "ICE", 0xff06080b, 0xff0c1117, 0xff131b24, 0xff22303e, 0xffdfeaf3, 0xff76889a, 0xff69c3ff, 0xffb7e3ff, 0xffffffff },
    { "RUST", 0xff0a0806, 0xff13100c, 0xff1c1712, 0xff3a2c1f, 0xffefe5d8, 0xff9a8a75, 0xffe2762f, 0xffffb070, 0xffffffff },
    { "PAPER", 0xffe9e4d8, 0xffdcd6c8, 0xffd0c9b9, 0xffa59d8b, 0xff121110, 0xff5d574d, 0xff121110, 0xff000000, 0xff000000 },
};
// the FM-1's 24 (firmware app/ui/common.c SKIN_NAME, SKIN_RGB: line, dim, gray, mid, hi, block, white), FLEXOR's
// (FLEXOR, 24 Nov 2026: "refais les équivalents des skins présents dans la machine pour les adapter à la VST")
static const char* const FM1_NAME[24] = { "CHROME", "FIRE&ICE", "CHOPPAH", "GLOUPI", "TESS", "OUTRUN", "THUNDER", "CYBERPUNK",
    "MC SALO", "ACID", "RED", "GARRY", "PIP-BOY", "NEON", "BLACKLIGHT", "PATINA", "SYNTHWAVE", "MIAMI", "ELECTRIC", "ARCTIC",
    "BREADCRUMBZ", "IRIS", "GLUCKSMAN", "MAXENCE" };
static const uint8_t FM1_RGB[24][7][3] = {
    {{41, 41, 48}, {101, 101, 116}, {198, 198, 208}, {255, 40, 60}, {235, 240, 250}, {21, 21, 24}, {253, 254, 254}},
    {{36, 36, 48}, {89, 89, 116}, {191, 191, 208}, {255, 60, 40}, {130, 230, 255}, {18, 18, 24}, {242, 252, 255}},
    {{34, 34, 40}, {74, 74, 82}, {150, 150, 158}, {232, 136, 30}, {255, 184, 70}, {24, 24, 30}, {255, 255, 255}},
    {{48, 24, 20}, {100, 56, 44}, {190, 160, 150}, {230, 30, 60}, {255, 140, 20}, {24, 10, 8}, {255, 240, 220}},
    {{41, 14, 48}, {101, 39, 116}, {198, 159, 208}, {245, 60, 130}, {255, 196, 90}, {21, 7, 24}, {255, 249, 238}},
    {{21, 10, 48}, {54, 31, 116}, {169, 155, 208}, {180, 60, 255}, {255, 140, 40}, {10, 5, 24}, {255, 244, 234}},
    {{13, 17, 48}, {37, 47, 116}, {158, 165, 208}, {40, 120, 255}, {255, 240, 60}, {7, 9, 24}, {255, 254, 236}},
    {{10, 43, 48}, {30, 105, 116}, {154, 201, 208}, {0, 240, 255}, {255, 236, 40}, {5, 22, 24}, {255, 253, 234}},
    {{36, 48, 18}, {89, 116, 48}, {191, 208, 165}, {40, 255, 120}, {255, 225, 0}, {18, 24, 9}, {255, 252, 230}},
    {{46, 42, 14}, {98, 92, 40}, {196, 190, 140}, {255, 64, 28}, {255, 234, 0}, {22, 20, 6}, {255, 255, 228}},
    {{34, 34, 40}, {74, 74, 82}, {150, 150, 158}, {232, 76, 60}, {255, 132, 112}, {24, 24, 30}, {255, 255, 255}},
    {{24, 40, 20}, {60, 90, 50}, {160, 190, 150}, {170, 60, 255}, {190, 255, 40}, {10, 18, 8}, {235, 255, 220}},
    {{14, 44, 22}, {30, 100, 54}, {80, 180, 110}, {40, 220, 100}, {140, 255, 170}, {8, 28, 14}, {200, 255, 210}},
    {{48, 48, 48}, {116, 116, 116}, {208, 208, 208}, {20, 180, 60}, {60, 255, 40}, {24, 24, 24}, {236, 255, 234}},   // (NEON: its pink gone, the firmware's 0.9.83)
    {{28, 12, 48}, {71, 35, 116}, {179, 157, 208}, {60, 255, 100}, {200, 100, 255}, {14, 6, 24}, {250, 240, 255}},
    {{48, 41, 34}, {116, 101, 85}, {208, 198, 188}, {225, 115, 55}, {60, 225, 195}, {24, 21, 17}, {236, 252, 249}},
    {{44, 22, 82}, {96, 58, 150}, {186, 160, 226}, {255, 46, 170}, {40, 236, 255}, {20, 8, 44}, {255, 214, 236}},
    {{20, 40, 56}, {50, 90, 110}, {150, 190, 200}, {0, 210, 200}, {255, 110, 150}, {8, 22, 32}, {255, 240, 230}},
    {{16, 16, 48}, {44, 44, 116}, {163, 163, 208}, {255, 40, 150}, {70, 170, 255}, {8, 8, 24}, {236, 246, 255}},
    {{13, 22, 48}, {37, 58, 116}, {158, 172, 208}, {60, 130, 255}, {190, 245, 255}, {6, 11, 24}, {248, 254, 255}},
    {{34, 34, 40}, {74, 74, 82}, {150, 150, 158}, {196, 196, 202}, {236, 236, 240}, {24, 24, 30}, {255, 255, 255}},
    {{34, 32, 44}, {76, 72, 90}, {152, 148, 166}, {150, 100, 240}, {196, 166, 255}, {24, 22, 34}, {255, 255, 255}},
    {{21, 11, 48}, {56, 32, 116}, {170, 155, 208}, {255, 90, 200}, {140, 90, 255}, {11, 5, 24}, {244, 238, 255}},
    {{60, 12, 48}, {130, 36, 110}, {240, 170, 225}, {255, 0, 170}, {255, 70, 215}, {44, 0, 34}, {255, 232, 250}} };
static juce::Colour rgb(const uint8_t* c) { return juce::Colour(c[0], c[1], c[2]); }
// an FM-1 skin as the VST's: its block the panels (darker the ground), its rules the lines, its labels and white the
// text, its secondary the accent (the steps on), its values the vivid one (the lane edited, the values), its white
static Skin fromFm1(const char* name, const uint8_t (*c)[3])
{
    const auto line = rgb(c[0]), dim = rgb(c[1]), gray = rgb(c[2]), mid = rgb(c[3]), hi = rgb(c[4]), block = rgb(c[5]), white = rgb(c[6]);
    Skin k;
    k.name = name;
    k.bg = block.darker(0.55f);
    k.panel = block;
    k.panel2 = block.interpolatedWith(line, 0.5f);
    k.line = line.interpolatedWith(dim, 0.35f);
    k.ink = gray.interpolatedWith(white, 0.55f);
    k.dim = dim.interpolatedWith(gray, 0.55f);
    k.accent = mid;
    k.hot = hi;
    k.white = white;
    k.fm1 = true;
    return k;
}
static std::vector<Skin>& table()
{
    static std::vector<Skin> t;
    if (t.empty()) {
        for (auto& r : OWN)
            t.push_back({ r.name, juce::Colour(r.bg), juce::Colour(r.panel), juce::Colour(r.panel2), juce::Colour(r.line),
                          juce::Colour(r.ink), juce::Colour(r.dim), juce::Colour(r.accent), juce::Colour(r.hot), juce::Colour(r.white), false });
        for (int i = 0; i < 24; ++i) t.push_back(fromFm1(FM1_NAME[i], FM1_RGB[i]));
        t.push_back(perso());
    }
    return t;
}
static int skinNow = -1;
static int fm1Skin = 10;                 // (the FM-1's own, as its SETUP has it: "COMME LE FM-1")

int skinCount() { return (int) table().size(); }
int fm1First() { return (int) (sizeof OWN / sizeof OWN[0]); }
int persoIndex() { return skinCount() - 1; }
const Skin& skin(int i)
{
    if (i == FOLLOW_FM1) return table()[(size_t) (fm1First() + juce::jlimit(0, 23, fm1Skin))];
    return table()[(size_t) juce::jlimit(0, skinCount() - 1, i)];
}
int curIndex()
{
    if (skinNow == -1) {
        skinNow = settings().getUserSettings()->getIntValue("skin2", 0);
        if (skinNow != FOLLOW_FM1) skinNow = juce::jlimit(0, skinCount() - 1, skinNow);
    }
    return skinNow;
}
const Skin& cur() { return skin(curIndex()); }
void setSkin(int i)
{
    skinNow = i == FOLLOW_FM1 ? i : juce::jlimit(0, skinCount() - 1, i);
    settings().getUserSettings()->setValue("skin2", skinNow);
}
bool fm1SkinIs(int s)                    // the FM-1's skin changed: true when the VST follows it (to repaint)
{
    if (s < 0 || s > 23 || s == fm1Skin) return false;
    fm1Skin = s;
    return curIndex() == FOLLOW_FM1;
}
// PERSO: the player's own, each colour picked (kept in the settings as nine #rrggbb)
Skin perso()
{
    Skin k = { "PERSO", juce::Colour(0xff090909), juce::Colour(0xff111111), juce::Colour(0xff191919), juce::Colour(0xff2c2b29),
               juce::Colour(0xffe8e3d8), juce::Colour(0xff8a857b), juce::Colour(0xfff4efe4), juce::Colour(0xffffffff), juce::Colour(0xffffffff), false };
    const auto v = juce::StringArray::fromTokens(settings().getUserSettings()->getValue("skinPerso"), ",", "");
    juce::Colour* f[9] = { &k.bg, &k.panel, &k.panel2, &k.line, &k.ink, &k.dim, &k.accent, &k.hot, &k.white };
    for (int i = 0; i < 9 && i < v.size(); ++i) *f[i] = juce::Colour::fromString(v[i]);
    return k;
}
void setPerso(const Skin& k)
{
    const juce::Colour f[9] = { k.bg, k.panel, k.panel2, k.line, k.ink, k.dim, k.accent, k.hot, k.white };
    juce::StringArray v;
    for (auto& c : f) v.add(c.toString());
    settings().getUserSettings()->setValue("skinPerso", v.joinIntoString(","));
    table().back() = perso();
}

juce::ApplicationProperties& settings()
{
    static juce::ApplicationProperties p;
    static bool once = false;
    if (! once) {
        once = true;
        juce::PropertiesFile::Options o;
        o.applicationName = "LOAF BREAKER VST";
        o.filenameSuffix = ".settings";
        o.folderName = "LOAF BREAKER";
        o.osxLibrarySubFolder = "Application Support";
        p.setStorageParameters(o);
    }
    return p;
}

static juce::Typeface::Ptr face(const char* file)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String(BinaryData::originalFilenames[i]) == file) {
            int n = 0;
            const char* d = BinaryData::getNamedResource(BinaryData::namedResourceList[i], n);
            return juce::Typeface::createSystemTypefaceFor(d, (size_t) n);
        }
    return nullptr;
}
static juce::Image image(const char* file)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String(BinaryData::originalFilenames[i]) == file) {
            int n = 0;
            const char* d = BinaryData::getNamedResource(BinaryData::namedResourceList[i], n);
            return juce::ImageCache::getFromMemory(d, n);
        }
    return {};
}
// (FLEXOR, 24 Nov 2026: "la police d'écriture est illisible, ne pas mettre de police pixélisée": Barlow Condensed,
// OFL, clean and narrow; the sizes the call sites give are scaled to it here)
juce::Font label(float h)
{
    static auto t = face("BarlowCondensed-SemiBold.ttf");
    const float s = h * 1.45f;
    return (t ? juce::Font(juce::FontOptions(t).withHeight(s)) : juce::Font(juce::FontOptions(s))).withExtraKerningFactor(0.04f);
}
juce::Font labelBold(float h)
{
    static auto t = face("BarlowCondensed-Bold.ttf");
    const float s = h * 1.4f;
    return (t ? juce::Font(juce::FontOptions(t).withHeight(s)) : juce::Font(juce::FontOptions(s, juce::Font::bold))).withExtraKerningFactor(0.03f);
}
juce::Font value(float h)
{
    static auto t = face("BarlowCondensed-Medium.ttf");
    const float s = h * 1.0f;
    return t ? juce::Font(juce::FontOptions(t).withHeight(s)) : juce::Font(juce::FontOptions(s));
}
juce::Image face() { static auto i = image("loaflord_face.png"); return i; }
juce::Image title() { static auto i = image("loaflord_title.png"); return i; }

void grain(juce::Graphics& g, juce::Rectangle<int> r, float amount)
{
    // a 160 px tile: specks of light and dark, and a halftone dot grid barely there (the poster's print)
    static juce::Image tile;
    if (tile.isNull()) {
        tile = juce::Image(juce::Image::ARGB, 160, 160, true);
        juce::Random rnd(2623);
        juce::Image::BitmapData bd(tile, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 160; ++y)
            for (int x = 0; x < 160; ++x) {
                const float n = rnd.nextFloat();
                const bool dot = (x % 4 == 0) && (y % 4 == 0);
                juce::Colour c = n > 0.985f ? juce::Colours::white.withAlpha(0.10f)
                               : n < 0.03f ? juce::Colours::black.withAlpha(0.22f)
                               : dot ? juce::Colours::white.withAlpha(0.022f) : juce::Colours::transparentBlack;
                bd.setPixelColour(x, y, c);
            }
    }
    g.saveState();
    g.setOpacity(juce::jlimit(0.0f, 1.0f, amount));
    g.setTiledImageFill(tile, r.getX(), r.getY(), 1.0f);
    g.fillRect(r);
    g.restoreState();
}

void vignette(juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::ColourGradient cg(juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                            juce::Colours::black.withAlpha(0.32f), r.getX(), r.getY(), true);
    g.setGradientFill(cg);
    g.fillRect(r);
}

void panel(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t)
{
    const auto& s = cur();
    if (t.isNotEmpty())
        r = r.withTrimmedTop(9.0f);      // (room for the title's tab, inside the component)
    g.setColour(s.panel);
    g.fillRect(r);
    g.setColour(s.line);
    g.drawRect(r, 1.0f);
    // pixel corners: a notch at each corner
    g.setColour(s.bg);
    for (auto p : { r.getTopLeft(), r.getTopRight().translated(-2, 0), r.getBottomLeft().translated(0, -2),
                    r.getBottomRight().translated(-2, -2) })
        g.fillRect(p.x, p.y, 2.0f, 2.0f);
    if (t.isNotEmpty()) {
        auto f = label(10.0f);
        const float w = (float) juce::GlyphArrangement::getStringWidthInt(f, t) + 14.0f;
        juce::Rectangle<float> tab(r.getX() + 8.0f, r.getY() - 9.0f, w, 18.0f);
        g.setColour(s.bg);
        g.fillRect(tab);
        g.setColour(s.dim);
        g.setFont(f);
        g.drawText(t, tab, juce::Justification::centred);
    }
}

void button(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool on, bool hover, float textH)
{
    const auto& s = cur();
    g.setColour(on ? s.accent : hover ? s.panel2.brighter(0.08f) : s.panel2);
    g.fillRect(r);
    g.setColour(on ? s.hot : s.line);
    g.drawRect(r, 1.0f);
    g.setColour(on ? s.bg : hover ? s.hot : s.ink);
    fitText(g, text, r.reduced(4.0f, 1.0f), label(textH));
}

void fitText(juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Font f, juce::Justification j)
{
    float h = f.getHeight();
    while (h > 6.0f && juce::GlyphArrangement::getStringWidth(f.withHeight(h), t) > r.getWidth())
        h -= 1.0f;
    g.setFont(f.withHeight(h));
    g.drawText(t, r, j, false);
}

void card(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, const juce::String& sub, juce::Colour chip, bool lit)
{
    const auto& s = cur();
    g.setColour(s.panel);
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(lit ? s.accent.withAlpha(0.85f) : s.line);
    g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, lit ? 1.5f : 1.0f);
    float x = r.getX() + 14.0f;
    if (! chip.isTransparent()) {
        g.setColour(chip);
        g.fillRoundedRectangle(x, r.getY() + 16.0f, 8.0f, 8.0f, 2.0f);
        x += 16.0f;
    }
    if (title.isNotEmpty()) {
        g.setColour(s.ink);
        g.setFont(labelBold(11.0f).withExtraKerningFactor(0.12f));
        const float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), title);
        g.drawText(title, juce::Rectangle<float>(x, r.getY() + 6.0f, tw + 4.0f, 28.0f), juce::Justification::centredLeft, false);
        if (sub.isNotEmpty()) {
            g.setColour(s.dim.withAlpha(0.8f));
            fitText(g, sub, { x + tw + 12.0f, r.getY() + 6.0f, r.getRight() - (x + tw + 12.0f) - 110.0f, 28.0f }, label(8.5f),
                    juce::Justification::centredLeft);
        }
    }
}

juce::Colour layer(const juce::String& n)
{
    const auto t = n.toUpperCase();
    if (t.startsWith("SUB")) return juce::Colour(0xffe5484d);
    if (t.startsWith("PUNCH")) return juce::Colour(0xfff5a524);
    if (t.startsWith("GRIT")) return juce::Colour(0xff8fd14f);
    if (t.startsWith("CLICK")) return juce::Colour(0xff6cc4ff);
    if (t.startsWith("TOK")) return juce::Colour(0xffc99cff);
    return juce::Colours::transparentBlack;
}

juce::Colour rgb(float lo, float mi, float hi, float a)
{
    const float m = juce::jmax(lo, mi, hi, 1e-6f);
    auto c = [](float v, float g, float k) { return (uint8_t) juce::jlimit(30.0f, 255.0f, k * std::pow(juce::jlimit(0.0f, 1.0f, v), g)); };
    return juce::Colour(juce::jmax((uint8_t) 40, c(lo / m, 0.6f, 255.0f)), c(mi / m, 0.9f, 205.0f), c(hi / m, 0.8f, 255.0f)).withAlpha(a);
}

const char* waveStyleName(int st)
{
    static const char* N[WAVE_N] = { "ACTUEL", "RGB", "SERATO", "3 BAND", "BLUE", "FM-1" };
    return N[juce::jlimit(0, WAVE_N - 1, st)];
}

void waveColumn(juce::Graphics& g, int st, float x, float mid, float top, float bottom, float lo, float mi, float hi, float a)
{
    lo = juce::jmax(0.0f, lo), mi = juce::jmax(0.0f, mi), hi = juce::jmax(0.0f, hi);
    const float m = juce::jmax(lo, mi, hi);
    const float h = juce::jmax(1.0f, bottom - top);
    if (st == WAVE_ACTUEL || m <= 0.0f) {
        g.setColour(rgb(lo, mi, hi, a));
        g.fillRect(x, top, 1.0f, h);
        return;
    }
    if (st == WAVE_3BAND) {              // the lows blue behind, the mids orange, the highs white in front, each as tall
                                         // as it is against the strongest (the FM-1's)
        static const juce::uint8 C[3][3] = { { 36, 104, 255 }, { 255, 150, 40 }, { 245, 245, 245 } };
        static const float K[3] = { 1.5f, 0.95f, 0.45f };
        const float v[3] = { lo, mi, hi };
        for (int j = 0; j < 3; ++j) {
            const float k = juce::jlimit(0.0f, 1.0f, v[j] * K[j] / m);
            if (k <= 0.0f) continue;
            g.setColour(juce::Colour(C[j][0], C[j][1], C[j][2]).withAlpha(a));
            const float t = mid - (mid - top) * k, b = mid + (bottom - mid) * k;
            g.fillRect(x, t, 1.0f, juce::jmax(1.0f, b - t));
        }
        return;
    }
    juce::Colour c;
    if (st == WAVE_SERATO) {             // one hue: the lows red (weighed three times), the mids green, the highs blue
        const float A = 3.0f * lo, s = A + mi + hi, hue = s > 0.0f ? (mi * 120.0f + hi * 240.0f) / s : 0.0f;
        float r, gg, b;
        if (hue < 60.0f) { r = 1.0f; gg = hue / 60.0f; b = 0.0f; }
        else if (hue < 120.0f) { r = (120.0f - hue) / 60.0f; gg = 1.0f; b = 0.0f; }
        else if (hue < 180.0f) { r = 0.0f; gg = 1.0f; b = (hue - 120.0f) / 60.0f; }
        else { r = 0.0f; gg = (240.0f - hue) / 60.0f; b = 1.0f; }
        c = juce::Colour::fromFloatRGBA(r, gg, b, 1.0f);
    } else if (st == WAVE_BLUE) {        // dark blue to pale blue-white with the highs
        const float s = lo + mi + hi, r = s > 0.0f ? juce::jmin(1.0f, hi * 2.2f / s) : 0.0f;
        c = juce::Colour((juce::uint8) (20 + 150 * r), (juce::uint8) (70 + 155 * r), (juce::uint8) (210 + 45 * r));
    } else                               // RGB: the bands as they are, against the strongest
        c = juce::Colour::fromFloatRGBA(lo / m, mi / m, hi / m, 1.0f);
    g.setColour(c.withAlpha(a));
    g.fillRect(x, top, 1.0f, h);
}

void hatch(juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::Graphics::ScopedSaveState ss(g);
    g.reduceClipRegion(r.toNearestInt());
    g.setColour(juce::Colours::white.withAlpha(0.045f));
    for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 6.0f)
        g.drawLine(x, r.getBottom(), x + r.getHeight(), r.getY(), 2.0f);
    juce::ColourGradient gr(juce::Colours::black.withAlpha(0.0f), r.getRight() - 8.0f, 0.0f, juce::Colours::black.withAlpha(0.7f), r.getRight(), 0.0f, false);
    g.setGradientFill(gr);
    g.fillRect(r.withLeft(r.getRight() - 8.0f));
}

juce::String subtitle(const juce::String& tab, const juce::String& part)
{
    static const std::map<juce::String, juce::String> M = {
        { "PRESET", "LES KICKS INTEGRES" }, { "CLICK", "LE CLAQUEMENT" }, { "PUNCH", "LE COUP" }, { "STACK", "L'ATTAQUE D'UN AUTRE KICK" },
        { "TOK", "UN DEUXIEME CLIC" }, { "MIX", "LES COUCHES" }, { "SUB", "LE GRAVE" }, { "GRIT", "LE GRAIN" },
        { "SYNTH", "LA CHUTE DE HAUTEUR" }, { "SYNTH +", "GRONDEMENT, ASYMETRIE" }, { "MACRO", "TOUT D'UN COUP" },
        { "OUT", "LA SORTIE DU KICK" }, { "TAIL", "SON CARACTERE, SA NOTE, SA FIN" }, { "TAIL +", "FILTRE ET DRIVE DE LA QUEUE" },
        { "BASS", "GLISSE, BASSE INVERSEE" }, { "CLEAN MORPH", "VERS UN AUTRE KICK" }, { "TAIL GATE", "LA QUEUE HACHEE" },
        { "DISTO", "LA TOUCHE DST" }, { "DISTO +", "DRIVE, FORME, SCREECH" }, { "SCREAM", "LE HURLEMENT" },
        { "SUB FX", "L'EFFET DU GRAVE" }, { "PUNCH FX", "L'EFFET DU COUP" }, { "GRIT FX", "L'EFFET DU GRAIN" },
        { "CLICK FX", "L'EFFET DU CLIC" }, { "KNOBS", "K1-K4 DU FM-1" }, { "ASSIGN", "CE QUE TOURNENT K1-K4" },
        { "TOOLS", "MUTER, COMPARER, VOIR" }, { "EQ BANDS", "L'EQ DU KICK" }, { "EQ SHAPE", "SES FREQUENCES" },
        { "SOUND", "LE CORPS" }, { "SHAPE", "L'ATTAQUE, LA SALETE" }, { "SPACE", "CHUTE, MONTEE, PIECE" },
    };
    juce::ignoreUnused(tab);
    auto it = M.find(part);
    return it != M.end() ? it->second : juce::String();
}

} // namespace ui

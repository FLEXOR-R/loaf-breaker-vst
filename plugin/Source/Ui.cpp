#include "Ui.h"
#include "BinaryData.h"

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
    {{48, 48, 48}, {116, 116, 116}, {208, 208, 208}, {255, 30, 150}, {60, 255, 40}, {24, 24, 24}, {236, 255, 234}},
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

} // namespace ui

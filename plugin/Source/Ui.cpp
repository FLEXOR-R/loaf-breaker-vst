#include "Ui.h"
#include "BinaryData.h"

namespace ui {

static const Skin SKINS[] = {
    // LOAF LORD: black, bone, greys (the default)
    { "LOAF LORD", juce::Colour(0xff090909), juce::Colour(0xff111111), juce::Colour(0xff191919), juce::Colour(0xff2c2b29),
      juce::Colour(0xffe8e3d8), juce::Colour(0xff8a857b), juce::Colour(0xfff4efe4), juce::Colour(0xffffffff) },
    { "BLOOD", juce::Colour(0xff0a0707), juce::Colour(0xff130d0d), juce::Colour(0xff1c1212), juce::Colour(0xff3a2222),
      juce::Colour(0xffeee2df), juce::Colour(0xff94817d), juce::Colour(0xffd51f2e), juce::Colour(0xffff4a57) },
    { "TOXIC", juce::Colour(0xff070907), juce::Colour(0xff0e120d), juce::Colour(0xff151b13), juce::Colour(0xff26321f),
      juce::Colour(0xffe3eedb), juce::Colour(0xff7f9174), juce::Colour(0xff9fe32b), juce::Colour(0xffd4ff70) },
    { "ICE", juce::Colour(0xff06080b), juce::Colour(0xff0c1117), juce::Colour(0xff131b24), juce::Colour(0xff22303e),
      juce::Colour(0xffdfeaf3), juce::Colour(0xff76889a), juce::Colour(0xff69c3ff), juce::Colour(0xffb7e3ff) },
    { "RUST", juce::Colour(0xff0a0806), juce::Colour(0xff13100c), juce::Colour(0xff1c1712), juce::Colour(0xff3a2c1f),
      juce::Colour(0xffefe5d8), juce::Colour(0xff9a8a75), juce::Colour(0xffe2762f), juce::Colour(0xffffb070) },
    { "PAPER", juce::Colour(0xffe9e4d8), juce::Colour(0xffdcd6c8), juce::Colour(0xffd0c9b9), juce::Colour(0xffa59d8b),
      juce::Colour(0xff121110), juce::Colour(0xff5d574d), juce::Colour(0xff121110), juce::Colour(0xff000000) },
};
static int skinNow = -1;

int skinCount() { return (int) (sizeof SKINS / sizeof SKINS[0]); }
const Skin& skin(int i) { return SKINS[juce::jlimit(0, skinCount() - 1, i)]; }
int curIndex()
{
    if (skinNow < 0)
        skinNow = juce::jlimit(0, skinCount() - 1, settings().getUserSettings()->getIntValue("skin", 0));
    return skinNow;
}
const Skin& cur() { return skin(curIndex()); }
void setSkin(int i)
{
    skinNow = juce::jlimit(0, skinCount() - 1, i);
    settings().getUserSettings()->setValue("skin", skinNow);
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
juce::Font label(float h)
{
    static auto t = face("Silkscreen-Regular.ttf");
    return t ? juce::Font(juce::FontOptions(t).withHeight(h)) : juce::Font(juce::FontOptions(h));
}
juce::Font labelBold(float h)
{
    static auto t = face("Silkscreen-Bold.ttf");
    return t ? juce::Font(juce::FontOptions(t).withHeight(h)) : juce::Font(juce::FontOptions(h, juce::Font::bold));
}
juce::Font value(float h)
{
    static auto t = face("VT323-Regular.ttf");
    return t ? juce::Font(juce::FontOptions(t).withHeight(h)) : juce::Font(juce::FontOptions(h));
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
        auto f = label(11.0f);
        const float w = (float) juce::GlyphArrangement::getStringWidthInt(f, t) + 12.0f;
        juce::Rectangle<float> tab(r.getX() + 8.0f, r.getY() - 7.0f, w, 14.0f);
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

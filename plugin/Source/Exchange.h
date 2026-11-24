// LOAF BREAKER VST: managing the FM-1 and the VST (FLEXOR, 24 Nov 2026: "exporter ou importer des choses, patterns ou
// tracks ou kicks"; "écouter sur le PC les kicks de la machine que je vais remplacer ... importer des kicks de la
// machine, les nommer et les ranger en profitant de la limite moins grande ... une gestion totale de ma machine et de
// ma VST"): three columns, the VST's kicks, patterns and tracks, the library (kicks on the PC, as many as wanted, each a
// .lbkick file in Documents/LOAF BREAKER/Kicks and its folders), the FM-1's (its backup read over USB, or a .bak);
// every kick heard (>), the ones ticked sent, taken or kept, the rest of the other side as it was (core/fm1sync.c).
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class ExchangeView : public juce::Component
{
public:
    explicit ExchangeView(LoafBreakerProcessor& p);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    LoafBreakerProcessor& proc;
    lb_list_t vst {}, fm1 {};
    bool haveFm1 = false, fm1FromFile = false, fm1Changed = false;
    juce::MemoryBlock fm1Img;
    juce::String status = "Lis le FM-1 (USB, a l'arret) ou ouvre une sauvegarde .bak. > ecoute un kick, un clic le coche.";
    bool tick[2][3][64] {};              // [side 0 VST, 1 FM-1][kind][n]
    int dest[3] = { -1, -1, -1 };        // where they land: -1 the same numbers, else from n
    // the library
    struct Entry { juce::File file; bool folder; int depth; };
    std::vector<Entry> lib;
    std::set<juce::String> libTick;
    juce::File libSel;                   // (the folder new kicks go into)
    int libTop = 0;
    static juce::File libRoot();
    void scanLib();
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Point<float> mouse;
    void refreshVst();
    void readFm1();
    void openBak();
    void sendToFm1();
    void takeToVst();
    void keepInLib();
    void writeFm1();
    void libMenu(const Entry& e);
    std::vector<lb_pick_t> picks(int side) const;
    std::vector<juce::MemoryBlock> libBlobs() const;
    // the layout
    juce::Rectangle<float> kickCell(int side, int i) const;
    juce::Rectangle<float> hearBtn(juce::Rectangle<float> cell) const { return cell.withWidth(22.0f); }
    juce::Rectangle<float> patCell(int side, int i) const;
    juce::Rectangle<float> trkCell(int side, int i) const;
    juce::Rectangle<float> libRow(int i) const;
    int libRows() const { return 18; }
    juce::Rectangle<float> btn(int i) const;
    void say(const juce::String& s) { status = s; repaint(); }
};

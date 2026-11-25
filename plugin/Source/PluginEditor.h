// LOAF BREAKER VST: the window, its own interface (FLEXOR, 24 Nov 2026: "une refonte complète ... changer complètement
// de style et de façon d'être contrôlée ... des contrôles contextuels qui affichent ce qu'ils modifient ... cliquer sur des
// catégories qui contiennent elles-mêmes des onglets ... afficher en permanence la waveform, avoir des visuels").
// Drawn at 1440 x 900 and scaled to the window. The firmware does the work: the page's model (lbcore.c lb_model) says
// what is where, the clicks and keys become its buttons, knobs and keys.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include "PluginProcessor.h"
#include "KeyMap.h"

class LoafBreakerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LoafBreakerEditor(LoafBreakerProcessor&);
    ~LoafBreakerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;

    // what every part reads and does
    LoafBreakerProcessor& proc;
    lb_model_t m {};
    LoafBreakerProcessor::WaveView wave;
    LoafBreakerProcessor::PicView pic;           // the kick's or a drum's picture (lb_pic)
    int picZoom = 0;                             // its view in ms (0: the tab's own)
    bool padsOpen = false;                       // the pads open (big) or folded to a strip
    void togglePads();
    int heroKind() const;                        // what is drawn big: -1 nothing, 0 the kick, 1..3 a drum, 9 the break
    void tap(int button);                       // a button pressed and let go
    void hold(int button, bool down);
    void turn(int encoder, int detents);
    void key(bool black, int i, bool down);
    void goDomain(int d);
    void goSource(const juce::String& label);   // SOUNDS: KICK, DRUMS, BREAK
    void toast(const juce::String& t);
    juce::String toastText;
    double toastUntil = 0.0;
    bool showMachine = true;
    void menu();
    void toggleMachine();
    void machineSeparate(bool separate);         // the FM-1's screen in a window of its own
    void machineShow();
    bool machineSep = false;
    std::unique_ptr<struct MachineWindow> machineWin;
    void persoEditor();
    void openManager();
    void breakMenu();                            // the breaks by category, the playlist
    void playlistEditor();
    void nav(int command);                       // KeyAction::Nav*
    void keysEditor();
    void screenMode(int mode);                   // 0 the window, 1 full screen, 2 borderless full screen
    int screen = 0;
    juce::Rectangle<int> windowBounds;
    int titleH = 0;
    bool nativeBar = false;                          // VST, library, FM-1 (Exchange.h)

    // the page as menus: its parts (lbcore lb_scan), each setting's values once looked at (lb_options), kept by
    // domain | source | tab | part | knob
    LoafBreakerProcessor::PartsView parts;
    struct Opts { juce::StringArray v; bool wrap = false, more = false; };
    std::map<juce::String, Opts> opts;
    std::map<int, juce::String> asked;           // an answer awaited: its key
    juce::String pageKey() const;                // domain | source | tab
    juce::String optKey(const lb_part_t& p, int knob) const;
    void wantOptions(int part, int knob);
    // the sequencer's
    lb_seq_t seq {};
    uint8_t pats[64 * 40] {};
    bool seqView() const;                        // PATTERN, its grid (not GROOVE)
    bool songView() const;                       // SONG › TRACKS: the arranger

    struct Root;
    std::unique_ptr<Root> root;

private:
    void timerCallback() override;
    uint32_t modelSerial = 0, waveSerial = 0, picSerial = 0, partsSerial = 0, seqSerial = 0, patsSerial = 0, tracksSerial = 0;
    juce::String lastDom;
    int ctlShown = 0;                    // (the controller's state as last said)
    uint64_t ctlHeld = 0;
    std::unique_ptr<lb_opts_t> optBuf { new lb_opts_t() };
    std::set<int> heldKeys;
    juce::String pendingSource;
    int sourceTries = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoafBreakerEditor)
};

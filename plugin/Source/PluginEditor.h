// LOAF BREAKER VST: the window, its own interface (FLEXOR, 24 Nov 2026: "une refonte complète ... changer complètement
// de style et de façon d'être contrôlée ... des contrôles contextuels qui affichent ce qu'ils modifient ... cliquer sur des
// catégories qui contiennent elles-mêmes des onglets ... afficher en permanence la waveform, avoir des visuels").
// Drawn at 1440 x 900 and scaled to the window. The firmware does the work: the page's model (lbcore.c lb_model) says
// what is where, the clicks and keys become its buttons, knobs and keys.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
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

    struct Root;
    std::unique_ptr<Root> root;

private:
    void timerCallback() override;
    uint32_t modelSerial = 0, waveSerial = 0;
    std::set<int> heldKeys;
    juce::String pendingSource;
    int sourceTries = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoafBreakerEditor)
};

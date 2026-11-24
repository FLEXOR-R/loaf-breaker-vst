// LOAF BREAKER VST: the window. First step: the FM-1's screen, twice its size, and its whole panel played with the
// mouse (the 14 buttons, the 7 knobs: drag or wheel, the 27 keys) and the computer's keyboard; Volume and Host sync.
// The PC's own interface (sequencer, kick, song as pages of their own) comes next.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class LoafBreakerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LoafBreakerEditor(LoafBreakerProcessor&);
    ~LoafBreakerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    void timerCallback() override;
    LoafBreakerProcessor& proc;
    juce::Image screen { juce::Image::RGB, 240, 240, true };
    uint32_t serial = 0;
    std::vector<uint16_t> px = std::vector<uint16_t>(240 * 240);
    juce::Rectangle<int> screenArea;

    struct PanelButton;
    struct Encoder;
    struct Keys;
    juce::OwnedArray<juce::Component> panel;
    std::unique_ptr<Keys> keys;
    juce::Slider volume;
    juce::ToggleButton sync { "Host sync" };
    juce::Label status;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt;
    uint32_t heldKeys = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoafBreakerEditor)
};

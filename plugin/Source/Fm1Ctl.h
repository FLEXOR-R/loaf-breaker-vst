// LOAF BREAKER VST: the FM-1 as the plugin's controller (25 Nov 2026, FLEXOR: "contrôler les touches du clavier et les
// knobs du VST avec la machine ... les touches actionnées visibles sur le daw et sur la machine (rétroéclairage)").
// The firmware's CONTROLLER (0.9.79, app.c ctl_on): channel 16 both ways. To it: CC 127 1 (on, every second), 0 (off);
// notes, its LEDs (0-13 the buttons, 14 PLAY's green, 16-42 the keys; velocity 0 off, 1 glow, 2 mid, 3 lit). From it:
// notes 0-13 the buttons, 16-42 the keys; CC 16-22 the encoders (64 + detents); CC 25 then 24 MASTER; CC 127 1.
// This class owns the FM-1's own MIDI ports (each opened when free: a DAW may hold the input, its messages then come
// through the plugin's MIDI in) on a thread of its own; the audio thread reads and writes through two lock-free FIFOs.
#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <functional>

class Fm1Ctl : private juce::Thread, private juce::MidiInputCallback
{
public:
    Fm1Ctl() : juce::Thread("FM-1 controller") {}
    ~Fm1Ctl() override { stop(); }
    void start();
    void stop();                          // (CC 127 0 sent first: the FM-1 itself again at once)
    bool running() const { return isThreadRunning(); }
    std::function<bool()> paused;         // the backup link busy: the ports left to it

    // the audio thread
    bool popIn(uint32_t& msg);            // a channel-16 message from the FM-1's own port: status | d1 << 8 | d2 << 16
    void pushOut(uint32_t msg);           // one to the FM-1 (dropped when its port is not open)
    std::atomic<bool> inOpen { false }, outOpen { false };

private:
    void run() override;
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void openPorts();
    void closePorts();

    std::unique_ptr<juce::MidiInput> in;
    std::unique_ptr<juce::MidiOutput> out;
    juce::AbstractFifo inF { 1024 }, outF { 1024 };
    uint32_t inB[1024] {}, outB[1024] {};
};

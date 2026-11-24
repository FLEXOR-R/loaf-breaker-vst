// LOAF BREAKER VST: the FM-1 over USB-MIDI, fm1-base's backup protocol (device/backup.c; tools/fm1_backup.py):
// backup (cmd 0x42 with the app's magic: the image streamed back, 0x43 its end) and restore (cmd 0x41 with the image's
// length and CRC-32: the FM-1 then pulls it with read requests and asks on its screen, SEL yes / HOME no). In the
// update's framing (felucca-bsp ota.c): F0 pack7(00 59 cmd len24 body ~sum) F7. On its own thread.
#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <functional>

class Fm1Link : private juce::Thread, private juce::MidiInputCallback
{
public:
    // progress 0..100 (-1: a message only), the message; done: ok, the message, the image (a backup's)
    using Progress = std::function<void(int, const juce::String&)>;
    using Done = std::function<void(bool, const juce::String&, const juce::MemoryBlock&)>;
    Fm1Link() : juce::Thread("FM-1 link") {}
    ~Fm1Link() override { stopThread(4000); }
    bool busy() const { return isThreadRunning(); }
    void fetch(Progress p, Done d);                          // the FM-1's backup
    void send(const juce::MemoryBlock& image, Progress p, Done d);   // a restore
    static juce::String portName();                          // the FM-1's port, or empty

private:
    void run() override;
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    bool open(juce::String& why);
    void close();
    bool nextSysex(std::vector<uint8_t>& out, int ms);        // the next SysEx's bytes, F0..F7 off
    void sendFrame(uint8_t cmd, const std::vector<uint8_t>& body);
    void sendResponse(uint32_t addr, const uint8_t* data, size_t n, uint8_t fl);
    void doFetch();
    void doSend();
    void progress(int pct, const juce::String& t);
    void finish(bool ok, const juce::String& t, const juce::MemoryBlock& img = {});

    std::unique_ptr<juce::MidiInput> in;
    std::unique_ptr<juce::MidiOutput> out;
    juce::CriticalSection qLock;
    std::deque<std::vector<uint8_t>> queue;
    juce::WaitableEvent got;
    bool sending = false;
    juce::MemoryBlock image;
    Progress onProgress;
    Done onDone;
};
